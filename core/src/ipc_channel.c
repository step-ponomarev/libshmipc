#include "ipc_utils.h"
#include <errno.h>
#include <pthread.h>
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_channel.h>
#include <shmipc/ipc_common.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define WAIT_EXPAND_FACTOR 2
#define NANOS_PER_SEC 1000000000ULL

#define CHANNEL_HEADER_SIZE_ALIGNED                                            \
  ALIGN_UP_BY_CACHE_LINE(sizeof(IpcChannelHeader))

typedef struct IpcChannelHeader {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} IpcChannelHeader;

struct IpcChannel {
  IpcChannelHeader *header;
  IpcBuffer *buffer;
};

static IpcChannelReadResult _try_read(IpcChannel *, IpcEntry *);
static bool _is_error_status(const IpcStatus);
static bool _is_retry_status(const IpcStatus);

inline uint64_t ipc_channel_get_memory_overhead(void) {
  return CHANNEL_HEADER_SIZE_ALIGNED + ipc_buffer_get_memory_overhead();
}

inline uint64_t ipc_channel_get_min_size(void) {
  return ipc_channel_get_memory_overhead() + ipc_buffer_get_min_size();
}

uint64_t ipc_channel_suggest_size(size_t desired_capacity) {
  const uint64_t min_size = ipc_channel_get_min_size();
  const uint64_t overhead = ipc_channel_get_memory_overhead();

  if (desired_capacity + overhead < min_size) {
    return min_size;
  }

  const uint64_t aligned_capacity = find_next_power_of_2(desired_capacity);
  return aligned_capacity + overhead;
}

IpcChannelOpenResult ipc_channel_create(void *mem, const size_t size) {
  const size_t min_total = ipc_channel_get_memory_overhead();
  IpcChannelOpenError error = {
      .requested_size = size, .min_size = min_total, .sys_errno = 0};

  if (mem == NULL) {
    return IpcChannelOpenResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: mem is NULL", error);
  }

  if (size == 0) {
    return IpcChannelOpenResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer size is 0", error);
  }

  uint8_t *buffer_memory = ((uint8_t *)mem) + CHANNEL_HEADER_SIZE_ALIGNED;
  const IpcBufferCreateResult buffer_result = ipc_buffer_create(
      (void *)buffer_memory, (size_t)size - CHANNEL_HEADER_SIZE_ALIGNED);
  if (IpcBufferCreateResult_is_error(buffer_result)) {
    error.requested_size = size;
    error.sys_errno = buffer_result.error.body.sys_errno;
    return IpcChannelOpenResult_error_body(buffer_result.ipc_status,
                                           buffer_result.error.detail, error);
  }

  // TODO: destroy buffer on any error below
  IpcChannel *channel = (IpcChannel *)malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    free(buffer_result.result);
    error.requested_size = size;
    return IpcChannelOpenResult_error_body(
        IPC_ERR_SYSTEM, "system error: channel allocation failed", error);
  }

  channel->header = (IpcChannelHeader *)mem;
  channel->buffer = buffer_result.result;

  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

  if (pthread_mutex_init(&channel->header->mutex, &mutex_attr) != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    free(channel);
    error.requested_size = size;
    error.sys_errno = errno;
    return IpcChannelOpenResult_error_body(
        IPC_ERR_SYSTEM, "system error: pthread_mutex_init failed", error);
  }
  pthread_mutexattr_destroy(&mutex_attr);

  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

  if (pthread_cond_init(&channel->header->cond, &cond_attr) != 0) {
    pthread_condattr_destroy(&cond_attr);
    pthread_mutex_destroy(&channel->header->mutex);
    free(channel);
    error.requested_size = size;
    error.sys_errno = errno;
    return IpcChannelOpenResult_error_body(
        IPC_ERR_SYSTEM, "system error: pthread_cond_init failed", error);
  }
  pthread_condattr_destroy(&cond_attr);

  return IpcChannelOpenResult_ok(IPC_OK, channel);
}

IpcChannelConnectResult ipc_channel_connect(void *mem) {
  const size_t min_total = ipc_channel_get_memory_overhead();
  IpcChannelConnectError error = {.min_size = min_total};

  if (mem == NULL) {
    return IpcChannelConnectResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: mem is NULL", error);
  }

  IpcChannel *channel = (IpcChannel *)malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    return IpcChannelConnectResult_error_body(
        IPC_ERR_SYSTEM, "system error: channel allocation failed", error);
  }

  uint8_t *buffer_memory = ((uint8_t *)mem) + CHANNEL_HEADER_SIZE_ALIGNED;
  const IpcBufferAttachResult buffer_result =
      ipc_buffer_attach((void *)buffer_memory);
  if (IpcBufferAttachResult_is_error(buffer_result)) {
    free(channel);
    return IpcChannelConnectResult_error_body(
        buffer_result.ipc_status, buffer_result.error.detail, error);
  }

  channel->header = (IpcChannelHeader *)mem;
  channel->buffer = buffer_result.result;

  return IpcChannelConnectResult_ok(IPC_OK, channel);
}

IpcChannelDestroyResult ipc_channel_destroy(IpcChannel *channel) {
  IpcChannelDestroyError error = {._unit = false};

  if (channel == NULL) {
    return IpcChannelDestroyResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelDestroyResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  // TODO: destroy buffer?
  free(channel->buffer);
  free(channel);
  return IpcChannelDestroyResult_ok(IPC_OK);
}

IpcChannelWriteResult ipc_channel_write(IpcChannel *channel, const void *data,
                                        const size_t size) {
  IpcChannelWriteError error = {.offset = 0,
                                .requested_size = (size_t)size,
                                .available_contiguous = 0,
                                .buffer_size = 0};

  if (channel == NULL) {
    return IpcChannelWriteResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", error);
  }

  if (channel->buffer == NULL) {
    return IpcChannelWriteResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", error);
  }

  const IpcBufferWriteResult write_result =
      ipc_buffer_write(channel->buffer, data, size);
  if (IpcBufferWriteResult_is_error(write_result)) {
    if (IpcBufferWriteResult_is_error_has_body(write_result.error)) {
      const IpcBufferWriteError b = write_result.error.body;
      error.offset = b.offset;
      error.requested_size = b.requested_size;
      error.available_contiguous = b.available_contiguous;
      error.buffer_size = b.buffer_size;
    }

    if (write_result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS) {
      pthread_cond_broadcast(&channel->header->cond);
    }

    return IpcChannelWriteResult_error_body(write_result.ipc_status,
                                            write_result.error.detail, error);
  }

  pthread_cond_broadcast(&channel->header->cond);

  return IpcChannelWriteResult_ok(write_result.ipc_status);
}

IpcChannelTryReadResult ipc_channel_try_read(IpcChannel *channel,
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

IpcChannelReadResult ipc_channel_read(IpcChannel *channel, IpcEntry *dest,
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
    if (IpcBufferPeekResult_is_error(peek_result) &&
        !_is_retry_status(peek_result.ipc_status)) {
      free(read_entry.payload);
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(peek_result.ipc_status,
                                             peek_result.error.detail, error);
    }

    struct timespec curr_time;
    if (clock_gettime(CLOCK_MONOTONIC, &curr_time) != 0) {
      free(read_entry.payload);
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(
          IPC_ERR_SYSTEM, "system error: clock_gettime failed", error);
    }

    const uint64_t curr_ns = ipc_timespec_to_nanos(&curr_time);
    if (curr_ns - start_ns > timeout_ns) {
      free(read_entry.payload);
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(IPC_ERR_TIMEOUT,
                                             "timeout: read timed out", error);
    }

    if (_is_retry_status(peek_result.ipc_status)) {
      if (pthread_cond_timedwait(&channel->header->cond,
                                 &channel->header->mutex, timeout) == EINVAL) {
        error.sys_errno = EINVAL;
        // not timeout
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: illegal pthrread state", error);
      }

      // Continue loop to check data again
      continue;
    }

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
  }
}

IpcChannelPeekResult ipc_channel_peek(const IpcChannel *channel,
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

IpcChannelSkipResult ipc_channel_skip(IpcChannel *channel,
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

IpcChannelSkipForceResult ipc_channel_skip_force(IpcChannel *channel) {
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

static IpcChannelReadResult _try_read(IpcChannel *channel, IpcEntry *dest) {
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
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: allocation failed", error);
      }

      dest->size = peek_entry.size;
    } else if (dest->size < peek_entry.size) {
      void *new_buf = realloc(dest->payload, peek_entry.size);
      if (new_buf == NULL) {
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
  return status != IPC_OK && !_is_retry_status(status);
}

static inline bool _is_retry_status(const IpcStatus status) {
  return status == IPC_ERR_NOT_READY || status == IPC_EMPTY ||
         status == IPC_ERR_CORRUPTED || status == IPC_ERR_LOCKED;
}
