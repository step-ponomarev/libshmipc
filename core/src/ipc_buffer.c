#include "ipc_utils.h"
#include <errno.h>
#include <shmipc/ipc_buffer.h>
#include "ipc_error_internal.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IPC_DATA_ALIGN 0x8

#define BUFFER_HEADER_SIZE_ALIGNED sizeof(ipc_buffer_header_t)
#define MIN_BUFFER_SIZE (BUFFER_HEADER_SIZE_ALIGNED + IPC_DATA_ALIGN)
#define UNLOCK(offset) (((offset) & (~(0x1))))
#define LOCK(offset) ((offset) | 0x1)

typedef struct {
    _Atomic uint64_t head;
    _Atomic uint64_t buffer_size; // TODO: static value read once
    uint8_t _r_padding[64 - 2 * sizeof(uint64_t)];

    _Atomic uint64_t tail;
    uint8_t _w_padding[64 - sizeof(uint64_t)];
} ipc_buffer_header_t;

typedef struct {
    uint64_t seq;
    uint64_t payload_size;
    uint64_t entry_size;
} entry_header_t;

struct ipc_buffer_t {
    ipc_buffer_header_t *header;
    uint8_t *data;
};

static uint64_t read_head(const ipc_buffer_t *buffer);

static uint64_t read_tail(const ipc_buffer_t *buffer);

static bool lock(_Atomic uint64_t *ref, uint64_t offset);

static bool unlock(_Atomic uint64_t *ref, uint64_t offset);

static bool is_locked(uint64_t offset);

static ipc_status_t skip_entry(ipc_buffer_t *buffer, uint64_t offset, const entry_header_t *header,
                               ipc_error_t *err);

static entry_header_t *read_entry_header(const ipc_buffer_t *buffer, uint64_t offset);


inline uint64_t ipc_buffer_memory_overhead(void) {
    return BUFFER_HEADER_SIZE_ALIGNED;
}

inline uint64_t ipc_buffer_min_size(void) {
    return MIN_BUFFER_SIZE;
}

uint64_t ipc_buffer_suggest_size(size_t desired_capacity) {
    const uint64_t overhead = ipc_buffer_memory_overhead();

    if (desired_capacity + overhead < MIN_BUFFER_SIZE) {
        return MIN_BUFFER_SIZE;
    }

    const uint64_t aligned_capacity = find_next_power_of_2(desired_capacity);
    return aligned_capacity + overhead;
}

ipc_status_t ipc_buffer_init(void *mem, size_t size, ipc_buffer_t **out, ipc_error_t *err) {
    ipc_error_init(err);
    ipc_buffer_t *res = NULL;

    if (out == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
    }

    *out = NULL;

    if (mem == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
    }

    if (size < MIN_BUFFER_SIZE) {
        return ipc_error_arg_size(
            err,
            IPC_ERR_CODE_TOO_SMALL_SIZE,
            (ipc_error_size_t){.requested_size = size, .limit = MIN_BUFFER_SIZE, .suggested_size = MIN_BUFFER_SIZE},
            "buffer size too small, use ipc_buffer_suggest_size"
        );
    }

    const uint64_t data_capacity = size - BUFFER_HEADER_SIZE_ALIGNED;
    if (!is_power_of_2(data_capacity)) {
        return ipc_error_arg_size(
            err,
            IPC_ERR_CODE_INVALID_CAPACITY,
            (ipc_error_size_t){
                .requested_size = size, .limit = MIN_BUFFER_SIZE,
                .suggested_size = ipc_buffer_suggest_size(data_capacity)
            },
            "size must be power of 2, use ipc_buffer_suggest_size"
        );
    }

    res = (ipc_buffer_t *) malloc(sizeof(ipc_buffer_t));
    if (res == NULL) {
        return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "buffer allocation failed", errno);
    }

    res->header = (ipc_buffer_header_t *) mem;
    res->data = (uint8_t *) mem + BUFFER_HEADER_SIZE_ALIGNED;

    atomic_init(&res->header->buffer_size, data_capacity);
    atomic_init(&res->header->head, 0);
    atomic_init(&res->header->tail, 0);

    *out = res;

    return IPC_STATUS_OK;
}

SHMIPC_API ipc_status_t ipc_buffer_attach(void *mem, ipc_buffer_t **out, ipc_error_t *err) {
    ipc_error_init(err);
    ipc_buffer_t *res = NULL;

    if (out == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
    }

    *out = NULL;

    if (mem == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
    }

    res = (ipc_buffer_t *) malloc(sizeof(ipc_buffer_t));
    if (res == NULL) {
        return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "buffer allocation failed", errno);
    }

    res->header = (ipc_buffer_header_t *) mem;
    res->data = (uint8_t *) mem + BUFFER_HEADER_SIZE_ALIGNED;

    *out = res;

    return IPC_STATUS_OK;
}

ipc_status_t ipc_buffer_detach(ipc_buffer_t *buffer, ipc_error_t *err) {
    ipc_error_init(err);

    if (buffer == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "buffer is null");
    }

    free(buffer);

    return IPC_STATUS_OK;
}

ipc_status_t ipc_buffer_write(ipc_buffer_t *buffer, const void *data, size_t size, ipc_error_t *err) {
ipc_buffer_write_start:;
    ipc_error_init(err);

    if (buffer == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "buffer is null");
    }

    if (data == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "data is null");
    }

    if (size == 0) {
        return ipc_error_arg(err, IPC_ERR_CODE_ZERO_SIZE, "data size is 0");
    }

    const uint64_t buf_size = atomic_load(&buffer->header->buffer_size);
    const uint64_t full_entry_size = ALIGN_UP(sizeof(entry_header_t) + size, IPC_DATA_ALIGN);
    if (full_entry_size > buf_size) {
        return ipc_error_arg_size(
            err,
            IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER,
            (ipc_error_size_t){.limit = buf_size, .requested_size = full_entry_size, .suggested_size = buf_size},
            "entry size exceeds buffer"
        );
    }

    uint64_t tail, rel_tail, space_to_wrap;
    bool use_placeholder = false;
    do {
        tail = atomic_load(&buffer->header->tail);
        if (is_locked(tail)) {
            return IPC_STATUS_BUSY;
        }

        rel_tail = RELATIVE(tail, buf_size);

        space_to_wrap = buf_size - rel_tail;
        const uint64_t head = UNLOCK(read_head(buffer));
        const uint64_t used_space = tail - head;
        const uint64_t free_space = buf_size - used_space;

        if (free_space < full_entry_size) {
            return IPC_STATUS_NO_SPACE;
        }

        // not enough contiguous space at buffer end for full entry; use placeholder to wrap and continue from start
        use_placeholder = space_to_wrap < full_entry_size + sizeof(entry_header_t);
    } while (!lock(&buffer->header->tail, tail)); // lock

    entry_header_t *header = (entry_header_t *) (buffer->data + rel_tail);
    if (use_placeholder) {
        header->payload_size = 0;
        header->entry_size = space_to_wrap;
    } else {
        void *dest = (uint8_t *) header + sizeof(entry_header_t);
        memcpy(dest, data, size);
        header->entry_size = full_entry_size;
        header->payload_size = size;
    }
    header->seq = tail;

    uint64_t expected_offset = LOCK(tail);
    // unlock
    if (!atomic_compare_exchange_strong(&buffer->header->tail, &expected_offset, tail + header->entry_size)) {
        // read->write strict seq_cst synch
        return ipc_error_internal_cas(
            err,
            IPC_ERR_CODE_OFFSET_CAS_FAILED,
            "unexpected tail value during commit",
            (ipc_error_cas_t){
                .target = IPC_CAS_TARGET_TAIL, .expected_offset = LOCK(tail), .actual_offset = expected_offset,
                .desired_offset = tail + header->entry_size
            }
        );
    }

    if (use_placeholder) {
        goto ipc_buffer_write_start;
    }

    return IPC_STATUS_OK;
}

ipc_status_t ipc_buffer_read(ipc_buffer_t *buffer, ipc_entry_t *dest, ipc_error_t *err) {
ipc_buffer_read_start:;
    ipc_error_init(err);

    if (buffer == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "buffer is null");
    }

    if (dest == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "dest is null");
    }

    uint64_t head;
    do {
        head = read_head(buffer);
        if (is_locked(head)) {
            return IPC_STATUS_BUSY;
        }

        if (UNLOCK(read_tail(buffer)) == head) {
            // read->write seq_cst synch
            return IPC_STATUS_EMPTY;
        }
    } while (!lock(&buffer->header->head, head));

    const entry_header_t *header = read_entry_header(buffer, head);
    if (header->seq != head) {
        return ipc_error_internal_corrupted(
            err,
            "corrupted entry, unexpected seq",
            head
        );
    }

    const bool is_placeholder = header->payload_size == 0;
    if (is_placeholder) {
        const ipc_status_t status = skip_entry(buffer, head, header, err);
        if (status != IPC_STATUS_OK) {
            return status;
        }

        goto ipc_buffer_read_start;
    }

    const size_t dst_cap = dest->size;
    if (dst_cap < header->payload_size) {
        if (!unlock(&buffer->header->head, head)) {
            return ipc_error_internal_cas(
                err,
                IPC_ERR_CODE_OFFSET_CAS_FAILED,
                "unexpected head value during commit",
                (ipc_error_cas_t){
                    .target = IPC_CAS_TARGET_HEAD,
                    .expected_offset = LOCK(head),
                    .actual_offset = read_head(buffer),
                    .desired_offset = head
                }
            );
        }

        return ipc_error_arg_capacity(
            err,
            (ipc_error_capacity_t){.provided_capacity = dst_cap, .required_capacity = header->payload_size},
            "provided capacity is too small"
        );
    }

    const uint64_t rel_offset = RELATIVE(head, atomic_load(&buffer->header->buffer_size));
    memcpy(dest->payload, buffer->data + rel_offset + sizeof(entry_header_t), header->payload_size);
    dest->offset = head;
    dest->size = header->payload_size;

    uint64_t expected_offset = LOCK(head);
    if (!atomic_compare_exchange_strong(&buffer->header->head, &expected_offset, head + header->entry_size)) {
        // read->write strict seq_cst synch
        dest->offset = 0;
        dest->size = 0;

        return ipc_error_internal_cas(
            err,
            IPC_ERR_CODE_OFFSET_CAS_FAILED,
            "unexpected head value during commit",
            (ipc_error_cas_t){
                .target = IPC_CAS_TARGET_HEAD, .expected_offset = expected_offset, .actual_offset = expected_offset,
                .desired_offset = head + header->entry_size
            }
        );
    }

    return IPC_STATUS_OK;
}

ipc_status_t ipc_buffer_next_entry_size(ipc_buffer_t *buffer, size_t *out_size, ipc_error_t *err) {
ipc_buffer_next_size_start:;
    ipc_error_init(err);

    if (buffer == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "buffer is null");
    }

    if (out_size == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out_size is null");
    }

    *out_size = 0;
    uint64_t head;
    do {
        head = read_head(buffer);
        if (is_locked(head)) {
            return IPC_STATUS_BUSY;
        }

        if (UNLOCK(read_tail(buffer)) == head) {
            // read->write seq_cst synch
            return IPC_STATUS_EMPTY;
        }
    } while (!lock(&buffer->header->head, head));

    const entry_header_t *header = read_entry_header(buffer, head);
    if (header->seq != head) {
        return ipc_error_internal_corrupted(
            err,
            "corrupted entry, unexpected seq",
            head
        );
    }

    const bool is_placeholder = header->payload_size == 0;
    if (is_placeholder) {
        const ipc_status_t status = skip_entry(buffer, head, header, err);
        if (status != IPC_STATUS_OK) {
            return status;
        }

        goto ipc_buffer_next_size_start;
    }

    const size_t entry_size = header->payload_size;
    if (!unlock(&buffer->header->head, head)) {
        return ipc_error_internal_cas(
            err,
            IPC_ERR_CODE_OFFSET_CAS_FAILED,
            "unexpected head value during commit",
            (ipc_error_cas_t){
                .target = IPC_CAS_TARGET_HEAD,
                .expected_offset = LOCK(head),
                .actual_offset = read_head(buffer),
                .desired_offset = head
            }
        );
    }

    *out_size = entry_size;

    return IPC_STATUS_OK;
}

static ipc_status_t skip_entry(ipc_buffer_t *buffer, uint64_t offset, const entry_header_t *header, ipc_error_t *err) {
    ipc_error_init(err);

    if (header == NULL) {
        return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "header is null");
    }

    const uint64_t desired_offset = offset + header->entry_size;
    uint64_t expected_offset = LOCK(offset);

    if (!atomic_compare_exchange_strong(&buffer->header->head, &expected_offset, desired_offset)) {
        return ipc_error_internal_cas(
            err,
            IPC_ERR_CODE_OFFSET_CAS_FAILED,
            "unexpected head value during commit",
            (ipc_error_cas_t){
                .target = IPC_CAS_TARGET_HEAD,
                .expected_offset = LOCK(offset),
                .actual_offset = expected_offset,
                .desired_offset = desired_offset
            }
        );
    }

    return IPC_STATUS_OK;
}

static uint64_t read_head(const ipc_buffer_t *buffer) {
    return atomic_load(&buffer->header->head);
}

// strict seq_cst for entry synchronization
static uint64_t read_tail(const ipc_buffer_t *buffer) {
    return atomic_load(&buffer->header->tail);
}

static bool is_locked(const uint64_t offset) { return LOCK(offset) == offset; }

static bool lock(_Atomic uint64_t *ref, const uint64_t offset) {
    uint64_t expected = UNLOCK(offset);
    return atomic_compare_exchange_strong(ref, &expected, LOCK(offset));
}

static bool unlock(_Atomic uint64_t *ref, const uint64_t offset) {
    uint64_t expected = LOCK(offset);
    return atomic_compare_exchange_strong(ref, &expected, UNLOCK(offset));
}

static entry_header_t *read_entry_header(const ipc_buffer_t *buffer, uint64_t offset) {
    const uint64_t unlocked_offset = UNLOCK(offset);
    const uint64_t buf_size = atomic_load(&buffer->header->buffer_size);
    const uint64_t rel_offset = RELATIVE(unlocked_offset, buf_size);

    return (entry_header_t *) (buffer->data + rel_offset);
}
