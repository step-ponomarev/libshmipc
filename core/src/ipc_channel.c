#include "ipc_futex.h"
#include "ipc_utils.h"
#include <errno.h>
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_channel.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "ipc_error_internal.h"

#define NANOS_PER_SEC ((uint64_t)1000000000ULL)

#define CHANNEL_HEADER_SIZE_ALIGNED ALIGN_UP_BY_CACHE_LINE(sizeof(ipc_channel_header))

typedef struct {
    _Atomic uint32_t notify;
    uint8_t _padding[64 - sizeof(uint32_t)];
} ipc_channel_header;

struct ipc_channel_t {
    ipc_channel_header *header;
    ipc_buffer_t *buffer;
};

static ipc_status_t try_read(ipc_channel_t *, ipc_entry_t *, ipc_error_t *err);

inline uint64_t ipc_channel_memory_overhead(void) {
    return CHANNEL_HEADER_SIZE_ALIGNED + ipc_buffer_memory_overhead();
}

inline uint64_t ipc_channel_min_size(void) {
    return CHANNEL_HEADER_SIZE_ALIGNED + ipc_buffer_min_size();
}

inline uint32_t ipc_channel_get_notify_signal(ipc_channel_t *channel) {
    return atomic_load(&channel->header->notify);
}

inline bool ipc_channel_is_retry_status(ipc_status_t status) {
    return status == IPC_STATUS_BUSY || status == IPC_STATUS_EMPTY;
}

uint64_t ipc_channel_suggest_size(size_t desired_capacity) {
    const uint64_t min_size = ipc_channel_min_size();
    const uint64_t overhead = ipc_channel_memory_overhead();

    if (desired_capacity + overhead < min_size) {
        return min_size;
    }

    return find_next_power_of_2(desired_capacity) + overhead;
}

ipc_status_t ipc_channel_init(void *mem, size_t size, ipc_channel_t **out, ipc_error_t *err) {
    ipc_error_init(err);
    ipc_channel_t *res = NULL;

    if (out == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
    }

    *out = NULL;

    if (mem == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
    }

    const uint64_t min_size = ipc_channel_min_size();
    if (size < min_size) {
        return ipc_error_arg_size(
            err,
            IPC_ERR_CODE_TOO_SMALL_SIZE,
            (ipc_error_size_t){.limit = min_size, .requested_size = size, .suggested_size = min_size},
            "channel size too small, use ipc_channel_suggest_size"
        );
    }

    uint8_t *buffer_memory = (uint8_t *) mem + CHANNEL_HEADER_SIZE_ALIGNED;
    ipc_buffer_t *buffer;
    ipc_status_t status = ipc_buffer_init(
        buffer_memory, size - CHANNEL_HEADER_SIZE_ALIGNED, &buffer, err);

    if (status != IPC_STATUS_OK) {
        return status;
    }

    res = malloc(sizeof(ipc_channel_t));
    if (res == NULL) {
        status = ipc_buffer_detach(buffer, err);
        if (status != IPC_STATUS_OK) {
            return status;
        }

        return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "channel allocation failed", errno);
    }

    res->header = (ipc_channel_header *) mem;
    res->buffer = buffer;

    atomic_init(&res->header->notify, 0);

    *out = res;

    return IPC_STATUS_OK;
}

ipc_status_t ipc_channel_attach(void *mem, ipc_channel_t **out, ipc_error_t *err) {
    ipc_error_init(err);
    ipc_channel_t *res = NULL;

    if (out == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
    }

    *out = NULL;

    if (mem == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
    }

    res = malloc(sizeof(ipc_channel_t));
    if (res == NULL) {
        return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "channel allocation failed", errno);
    }

    uint8_t *buffer_memory = (uint8_t *) mem + CHANNEL_HEADER_SIZE_ALIGNED;
    ipc_buffer_t *buffer = NULL;

    const ipc_status_t attach_status = ipc_buffer_attach(buffer_memory, &buffer, err);
    if (attach_status != IPC_STATUS_OK) {
        free(res);
        return attach_status;
    }

    res->header = (ipc_channel_header *) mem;
    res->buffer = buffer;

    *out = res;

    return IPC_STATUS_OK;
}

ipc_status_t ipc_channel_detach(ipc_channel_t *channel, ipc_error_t *err) {
    ipc_error_init(err);

    if (channel == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "channel is null");
    }

    const ipc_status_t status = ipc_buffer_detach(channel->buffer, err);
    if (status != IPC_STATUS_OK) {
        return status;
    }

    free(channel);

    return IPC_STATUS_OK;
}

ipc_status_t ipc_channel_write(ipc_channel_t *channel, const void *data, size_t size, ipc_error_t *err) {
    ipc_error_init(err);

    if (channel == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "channel is null");
    }

    const ipc_status_t status = ipc_buffer_write(channel->buffer, data, size, err);
    if (status == IPC_STATUS_NO_SPACE || status == IPC_STATUS_OK) {
        atomic_fetch_add(&channel->header->notify, 1);
        ipc_futex_wake_all(&channel->header->notify);
    }

    return status;
}

ipc_status_t ipc_channel_try_read(ipc_channel_t *channel, ipc_entry_t *dest, ipc_error_t *err) {
    ipc_error_init(err);

    if (channel == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "channel is null");
    }

    if (dest == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "dest is null");
    }

    ipc_entry_t read_entry = {.offset = 0, .payload = NULL, .size = 0};
    const ipc_status_t status = try_read(channel, &read_entry, err);
    if (status == IPC_STATUS_OK) {
        dest->payload = read_entry.payload;
        dest->size = read_entry.size;
        dest->offset = read_entry.offset;
    } else {
        free(read_entry.payload);
    }

    return status;
}

ipc_status_t ipc_channel_read(ipc_channel_t *channel, ipc_entry_t *dest, const struct timespec *timeout,
                              ipc_error_t *err) {
    ipc_error_init(err);

    if (channel == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "channel is null");
    }

    if (dest == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "dest is null");
    }

    if (timeout == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "timeout is null");
    }

    if (timeout->tv_nsec < 0 || timeout->tv_sec < 0) {
        return ipc_error_arg(err, IPC_ERR_CODE_INVALID_TIMEOUT, "timeout is invalid");
    }

    uint64_t start_ns = 0;
    uint64_t timeout_ns = 0;
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        return ipc_error_sys(
            err,
            IPC_ERR_CODE_GET_TIME,
            "clock_gettime failed",
            errno
        );
    }
    start_ns = ipc_timespec_to_nanos(&start_time);
    timeout_ns = ipc_timespec_to_nanos(timeout);

    ipc_entry_t read_entry = {.offset = 0, .payload = NULL, .size = 0};
    for (;;) {
        ipc_status_t status = try_read(channel, &read_entry, err);
        if (status == IPC_STATUS_OK) {
            dest->payload = read_entry.payload;
            dest->size = read_entry.size;
            dest->offset = read_entry.offset;
            return status;
        }

        if (!ipc_channel_is_retry_status(status)) {
            free(read_entry.payload);
            return status;
        }

        struct timespec curr_time;
        if (clock_gettime(CLOCK_MONOTONIC, &curr_time) != 0) {
            free(read_entry.payload);
            return ipc_error_sys(
                err,
                IPC_ERR_CODE_GET_TIME,
                "clock_gettime failed",
                errno
            );
        }

        const uint64_t curr_ns = ipc_timespec_to_nanos(&curr_time);
        const uint64_t elapsed_ns = curr_ns - start_ns;
        if (elapsed_ns >= timeout_ns) {
            free(read_entry.payload);
            return IPC_STATUS_TIMEOUT;
        }

        const uint64_t remaining_ns = timeout_ns - elapsed_ns;
        struct timespec remaining_timeout = {
            .tv_sec = remaining_ns / NANOS_PER_SEC,
            .tv_nsec =
            remaining_ns % NANOS_PER_SEC
        };

        uint32_t expected_notify = atomic_load(&channel->header->notify);
        int wait_res = ipc_futex_wait(&channel->header->notify, expected_notify,
                                      &remaining_timeout);
        if (wait_res != 0 && wait_res != ETIMEDOUT) {
            free(read_entry.payload);
            return ipc_error_sys(
                err,
                IPC_ERR_CODE_FUTEX,
                "futex is failed",
                wait_res
            );
        }
    }
}

static ipc_status_t try_read(ipc_channel_t *channel, ipc_entry_t *dest, ipc_error_t *err) {
    ipc_error_init(err);
    dest->size = 0;
    dest->offset = 0;

    for (;;) {
        size_t required_size;
        ipc_status_t status = ipc_buffer_next_entry_size(channel->buffer, &required_size, err);
        if (status != IPC_STATUS_OK) {
            return status;
        }

        if (dest->payload == NULL) {
            dest->payload = malloc(required_size);
            if (dest->payload == NULL) {
                return ipc_error_sys(
                    err,
                    IPC_ERR_CODE_ALLOCATION,
                    "failed entry allocation",
                    errno
                );
            }

            dest->size = required_size;
        } else if (dest->size < required_size) {
            void *new_buf = realloc(dest->payload, required_size);
            if (new_buf == NULL) {
                return ipc_error_sys(
                    err,
                    IPC_ERR_CODE_ALLOCATION,
                    "failed entry allocation",
                    errno
                );
            }
            dest->payload = new_buf;
            dest->size = required_size;
        }

        status = ipc_buffer_read(channel->buffer, dest, err);
        if (status == IPC_STATUS_ERROR) {
            if (err->kind == IPC_ERR_KIND_ARG && err->code == IPC_ERR_CODE_INVALID_CAPACITY) {
                continue;
            }

            return status;
        }

        return status;
    }
}
