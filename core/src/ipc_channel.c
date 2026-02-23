#include "ipc_futex.h"
#include "ipc_utils.h"
#include <errno.h>
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_channel.h>
#include <shmipc/ipc_common.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "ipc_error_internal.h"

#define WAIT_EXPAND_FACTOR 2
#define NANOS_PER_SEC 1000000000ULL

#define CHANNEL_HEADER_SIZE_ALIGNED                                            \
  ALIGN_UP_BY_CACHE_LINE(sizeof(IpcChannelHeader))

typedef struct IpcChannelHeader {
  _Atomic uint32_t notify;
  uint8_t __align[64 - sizeof(uint32_t)];
} IpcChannelHeader;

struct ipc_channel_t {
  IpcChannelHeader *header;
  ipc_buffer_t *buffer;
};

static IpcChannelReadResult _try_read(ipc_channel_t *, IpcEntry *);
static bool _is_error_status(const IpcStatus);

inline uint64_t ipc_channel_get_memory_overhead(void) {
  return CHANNEL_HEADER_SIZE_ALIGNED + ipc_buffer_get_memory_overhead();
}

inline uint64_t ipc_channel_get_min_size(void) {
  return CHANNEL_HEADER_SIZE_ALIGNED + ipc_buffer_get_min_size();
}

inline uint32_t ipc_channel_get_notify_signal(ipc_channel_t *channel) {
  return atomic_load(&channel->header->notify);
}

inline bool ipc_channel_is_retry_status(const IpcStatus status) {
  return status == IPC_ERR_NOT_READY || status == IPC_EMPTY ||
         status == IPC_ERR_LOCKED;
}

uint64_t ipc_channel_suggest_size(size_t desired_capacity) {
  const uint64_t min_size = ipc_channel_get_min_size();
  const uint64_t overhead = ipc_channel_get_memory_overhead();

  if (desired_capacity + overhead < min_size) {
    return min_size;
  }

  return find_next_power_of_2(desired_capacity) + overhead;
}

ipc_status_t ipc_channel_create(void *mem, size_t size, ipc_channel_t **out, ipc_error_t *err) {
  ipc_error_init(err);
  ipc_channel_t *res = NULL;

  if (out == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
  }

  *out = NULL;

  if (mem == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
  }

  const uint64_t min_size = ipc_channel_get_min_size();
  if (size < min_size) {
    return ipc_error_arg_size(
      err,
      IPC_ERR_CODE_TOO_SMALL_SIZE,
      (ipc_error_size_t){.min_size = min_size, .provided_size = size, .suggested_size = min_size},
      "channel size too small, use ipc_channel_suggest_size"
    );
  }

  uint8_t *buffer_memory = (uint8_t *) mem + CHANNEL_HEADER_SIZE_ALIGNED;
  ipc_buffer_t *buffer;
  const ipc_status_t status = ipc_buffer_create(
    buffer_memory, size - CHANNEL_HEADER_SIZE_ALIGNED, &buffer, err);

  if (status != IPC_STATUS_OK) {
    return status;
  }

  res = malloc(sizeof(ipc_channel_t));
  if (res == NULL) {
    free(buffer);
    return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "channel allocation failed", errno);
  }

  res->header = (IpcChannelHeader *) mem;
  res->buffer = buffer;

  atomic_init(&res->header->notify, 0);

  *out = res;

  return IPC_STATUS_OK;
}

ipc_status_t ipc_channel_connect(void *mem, ipc_channel_t** out, ipc_error_t* err) {
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

  uint8_t *buffer_memory = (uint8_t *)mem + CHANNEL_HEADER_SIZE_ALIGNED;
  ipc_buffer_t *buffer = NULL;

  const ipc_status_t attach_status = ipc_buffer_attach(buffer_memory, &buffer, err);
  if (attach_status != IPC_STATUS_OK) {
    free(res);
    return attach_status;
  }

  res->header = (IpcChannelHeader *)mem;
  res->buffer = buffer;

  *out = res;

  return IPC_STATUS_OK;
}

IpcChannelDestroyResult ipc_channel_destroy(ipc_channel_t *channel) {
  IpcChannelDestroyError error = {._unit = false};

  if (channel == NULL) {
    return IpcChannelDestroyResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelDestroyResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  free(channel->buffer);
  free(channel);
  return IpcChannelDestroyResult_ok(IPC_OK);
}

ipc_status_t ipc_channel_write(ipc_channel_t *channel, const void *data, size_t size, ipc_error_t *err) {
  ipc_error_init(err);

  if (channel == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "channel is null");
  }

  if (channel->buffer == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "channel->buffer is null");
  }

  return ipc_buffer_write(channel->buffer, data, size, err);
}

IpcChannelTryReadResult ipc_channel_try_read(ipc_channel_t *channel,
                                             IpcEntry *dest) {
  IpcChannelTryReadError error = {.offset = 0};

  if (channel == NULL) {
    return IpcChannelTryReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelTryReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcChannelTryReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  IpcEntry read_entry = {.offset = 0, .payload = NULL, .size = 0};
  const IpcChannelReadResult read_result = _try_read(channel, &read_entry);
  if (read_result.ipc_status == IPC_OK) {
    dest->payload = read_entry.payload;
    dest->size = read_entry.size;
    dest->offset = read_entry.offset;
  } else {
    free(read_entry.payload);
  }

  if (IpcChannelReadResult_is_error(read_result)) {
    error.offset = read_result.error.body.offset;
    return IpcChannelTryReadResult_error_body(read_result.ipc_status,
                                              read_result.error.detail, error);
  }

  return IpcChannelTryReadResult_ok(read_result.ipc_status);
}

IpcChannelReadResult ipc_channel_read(ipc_channel_t *channel, IpcEntry *dest,
                                      const struct timespec *timeout) {
  IpcChannelReadError error = {.offset = 0, .timeout_used = {0, 0}};

  if (channel == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  if (timeout == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: timeout is NULL", error);
  }

  if (timeout->tv_nsec < 0 || timeout->tv_sec < 0) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: timeout must be {timeout->tv_nsec >= 0 && "
        "timeout->tv_sec >= 0}",
        error);
  }

  error.timeout_used = *timeout;

  uint64_t start_ns = 0;
  uint64_t timeout_ns = 0;
  struct timespec start_time;
  if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
    error.sys_errno = errno;
    return IpcChannelReadResult_error_body(
        IPC_ERR_SYSTEM, "system error: clock_gettime failed", error);
  }
  start_ns = ipc_timespec_to_nanos(&start_time);
  timeout_ns = ipc_timespec_to_nanos(timeout);

  IpcEntry read_entry = {.offset = 0, .payload = NULL, .size = 0};
  for (;;) {
    IpcEntry peek_entry;
    const IpcBufferPeekResult peek_result =
        ipc_buffer_peek(channel->buffer, &peek_entry);

    if (peek_result.ipc_status == IPC_OK) {
      const IpcChannelReadResult read_result = _try_read(channel, &read_entry);
      if (_is_error_status(read_result.ipc_status)) {
        free(read_entry.payload);
        return read_result;
      }

      if (read_result.ipc_status == IPC_OK) {
        dest->payload = read_entry.payload;
        dest->size = read_entry.size;
        dest->offset = read_entry.offset;
        return read_result;
      }
    } else if (!ipc_channel_is_retry_status(peek_result.ipc_status)) {
      free(read_entry.payload);
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(peek_result.ipc_status,
                                             peek_result.error.detail, error);
    }

    struct timespec curr_time;
    if (clock_gettime(CLOCK_MONOTONIC, &curr_time) != 0) {
      free(read_entry.payload);
      error.sys_errno = errno;
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(
          IPC_ERR_SYSTEM, "system error: clock_gettime failed", error);
    }

    const uint64_t curr_ns = ipc_timespec_to_nanos(&curr_time);
    const uint64_t elapsed_ns = curr_ns - start_ns;
    if (elapsed_ns >= timeout_ns) {
      free(read_entry.payload);
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(IPC_ERR_TIMEOUT,
                                             "timeout: read timed out", error);
    }

    const uint64_t remaining_ns = timeout_ns - elapsed_ns;
    struct timespec remaining_timeout = {.tv_sec = remaining_ns / NANOS_PER_SEC,
                                         .tv_nsec =
                                             remaining_ns % NANOS_PER_SEC};

    uint32_t expected_notify = atomic_load(&channel->header->notify);
    int wait_res = ipc_futex_wait(&channel->header->notify, expected_notify,
                                  &remaining_timeout);
    if (wait_res != 0 && wait_res != ETIMEDOUT) {
      free(read_entry.payload);
      error.sys_errno = errno;
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(
          IPC_ERR_SYSTEM, "system error: futex wait failed", error);
    }
  }
}

IpcChannelPeekResult ipc_channel_peek(const ipc_channel_t *channel,
                                      IpcEntry *dest) {
  IpcChannelPeekError error = {.offset = 0};
  if (channel == NULL) {
    return IpcChannelPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelPeekResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcChannelPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  const IpcBufferPeekResult peek_result =
      ipc_buffer_peek(channel->buffer, dest);
  if (IpcBufferPeekResult_is_error(peek_result)) {
    error.offset = peek_result.error.body.offset;
    return IpcChannelPeekResult_error_body(peek_result.ipc_status,
                                           peek_result.error.detail, error);
  }

  return IpcChannelPeekResult_ok(peek_result.ipc_status);
}

IpcChannelSkipResult ipc_channel_skip(ipc_channel_t *channel,
                                      const uint64_t offset) {
  IpcChannelSkipError error = {.offset = offset};
  if (channel == NULL) {
    return IpcChannelSkipResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelSkipResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  const IpcBufferSkipResult skip_result =
      ipc_buffer_skip(channel->buffer, offset);
  if (IpcBufferSkipResult_is_error(skip_result)) {
    error.offset = skip_result.error.body.offset;
    return IpcChannelSkipResult_error_body(skip_result.ipc_status,
                                           skip_result.error.detail, error);
  }

  return IpcChannelSkipResult_ok(skip_result.ipc_status, skip_result.result);
}

IpcChannelSkipForceResult ipc_channel_skip_force(ipc_channel_t *channel) {
  IpcChannelSkipForceError error = {._unit = false};
  if (channel == NULL) {
    return IpcChannelSkipForceResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelSkipForceResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  const IpcBufferSkipForceResult skip_result =
      ipc_buffer_skip_force(channel->buffer);
  if (IpcBufferSkipForceResult_is_error(skip_result)) {
    return IpcChannelSkipForceResult_error_body(
        skip_result.ipc_status, skip_result.error.detail, error);
  }

  return IpcChannelSkipForceResult_ok(skip_result.ipc_status,
                                      skip_result.result);
}

static IpcChannelReadResult _try_read(ipc_channel_t *channel, IpcEntry *dest) {
  IpcChannelReadError error = {.offset = 0, .timeout_used = {0, 0}};

  if (channel == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: data is NULL", error);
  }

  for (;;) {
    IpcEntry peek_entry;
    const IpcBufferPeekResult peek_result =
        ipc_buffer_peek(channel->buffer, &peek_entry);

    if (IpcBufferPeekResult_is_error(peek_result)) {
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(peek_result.ipc_status,
                                             peek_result.error.detail, error);
    }

    if (peek_result.ipc_status != IPC_OK) {
      return IpcChannelReadResult_ok(peek_result.ipc_status);
    }

    if (dest->payload == NULL) {
      dest->payload = malloc(peek_entry.size);
      if (dest->payload == NULL) {
        error.offset = peek_entry.offset;
        error.sys_errno = errno;
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: allocation failed", error);
      }

      dest->size = peek_entry.size;
    } else if (dest->size < peek_entry.size) {
      void *new_buf = realloc(dest->payload, peek_entry.size);
      if (new_buf == NULL) {
        error.sys_errno = errno;
        error.offset = peek_entry.offset;
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: allocation failed", error);
      }
      dest->payload = new_buf;
      dest->size = peek_entry.size;
    }

    const IpcBufferReadResult read_result =
        ipc_buffer_read(channel->buffer, dest);

    if (IpcBufferReadResult_is_error(read_result)) {
      if (read_result.ipc_status == IPC_ERR_TOO_SMALL) {
        continue;
      }

      error.offset = dest->offset;
      return IpcChannelReadResult_error_body(read_result.ipc_status,
                                             read_result.error.detail, error);
    }

    return IpcChannelReadResult_ok(read_result.ipc_status);
  }
}

static inline bool _is_error_status(const IpcStatus status) {
  return status != IPC_OK && !ipc_channel_is_retry_status(status);
}
