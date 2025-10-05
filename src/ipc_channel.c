#include "ipc_utils.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_channel.h>
#include <stdlib.h>

#define WAIT_EXPAND_FACTOR 2

struct IpcChannel {
  IpcBuffer *buffer;
  IpcChannelConfiguration config;
};

static IpcChannelReadResult _read(IpcChannel *, IpcEntry *,
                                  const struct timespec *);
static IpcChannelReadResult _try_read(IpcChannel *, IpcEntry *);
static bool _sleep_and_expand_delay(struct timespec *, const long);
static bool _is_valid_config(const IpcChannelConfiguration);
static bool _is_timeout_valid(const struct timespec *);
static bool _is_error_status(const IpcStatus);
static bool _is_retry_status(const IpcStatus);

inline uint64_t ipc_channel_align_size(size_t size) {
  return ipc_buffer_align_size(size);
}

IpcChannelResult ipc_channel_create(void *mem, const size_t size,
                                    const IpcChannelConfiguration config) {
  const size_t min_total = ipc_buffer_align_size(2);
  IpcChannelOpenError error = {
      .requested_size = size, .min_size = min_total, .config = config};

  if (mem == NULL) {
    return IpcChannelResult_error_body(IPC_ERR_INVALID_ARGUMENT,
                                       "invalid argument: mem is NULL", error);
  }

  if (size == 0) {
    return IpcChannelResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer size is 0", error);
  }

  if (!_is_valid_config(config)) {
    error.requested_size = size;
    return IpcChannelResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: valid config must be {start_sleep_ns > 0 && "
        "max_round_trips > 0 && max_sleep_ns > 0 && max_sleep_ns >= "
        "start_sleep_ns}",
        error);
  }

  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem, (size_t)size);
  if (IpcBufferCreateResult_is_error(buffer_result)) {
    error.requested_size = size;
    return IpcChannelResult_error_body(buffer_result.ipc_status,
                                       buffer_result.error.detail, error);
  }

  IpcChannel *channel = (IpcChannel *)malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    error.requested_size = size;
    return IpcChannelResult_error_body(
        IPC_ERR_SYSTEM, "system error: channel allocation failed", error);
  }

  channel->buffer = buffer_result.result;
  channel->config = config;

  return IpcChannelResult_ok(IPC_OK, channel);
}

IpcChannelConnectResult
ipc_channel_connect(void *mem, const IpcChannelConfiguration config) {
  const size_t min_total = ipc_buffer_align_size(2);
  IpcChannelConnectError error = {.min_size = min_total, .config = config};

  if (mem == NULL) {
    return IpcChannelConnectResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: mem is NULL", error);
  }

  if (!_is_valid_config(config)) {
    return IpcChannelConnectResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: valid config must be {start_sleep_ns > 0 && "
        "max_round_trips > 0 && max_sleep_ns > 0 && max_sleep_ns >= "
        "start_sleep_ns}",
        error);
  }

  IpcChannel *channel = (IpcChannel *)malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    return IpcChannelConnectResult_error_body(
        IPC_ERR_SYSTEM, "system error: channel allocation failed", error);
  }

  const IpcBufferAttachResult buffer_result = ipc_buffer_attach(mem);
  if (IpcBufferAttachResult_is_error(buffer_result)) {
    free(channel);
    return IpcChannelConnectResult_error_body(
        buffer_result.ipc_status, buffer_result.error.detail, error);
  }

  channel->buffer = buffer_result.result;
  channel->config = config;

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

    return IpcChannelWriteResult_error_body(write_result.ipc_status,
                                            write_result.error.detail, error);
  }

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

IpcChannelReadResult ipc_channel_read(IpcChannel *channel, IpcEntry *dest) {
  return _read(channel, dest, NULL);
}

IpcChannelReadWithTimeoutResult
ipc_channel_read_with_timeout(IpcChannel *channel, IpcEntry *dest,
                              const struct timespec *timeout) {
  const IpcChannelReadResult read_result = _read(channel, dest, timeout);
  if (IpcChannelReadResult_is_ok(read_result)) {
    return IpcChannelReadWithTimeoutResult_ok(read_result.ipc_status);
  }

  IpcChannelReadWithTimeoutError body = {
      .offset = read_result.error.body.offset,
      .timeout_used = timeout != NULL ? *timeout : (struct timespec){0, 0}};

  return IpcChannelReadWithTimeoutResult_error_body(
      read_result.ipc_status, read_result.error.detail, body);
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

  const IpcBufferSkipResult skip_result = ipc_buffer_skip(channel->buffer, offset);
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

static IpcChannelReadResult _read(IpcChannel *channel, IpcEntry *dest,
                                  const struct timespec *timeout) {
  IpcChannelReadError error = {.offset = 0};
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

  if (!_is_timeout_valid(timeout)) {
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: valid timeout must be "
        "{timeout == NULL || (timeout->tv_nsec >= 0 && timeout->tv_sec >= 0)}",
        error);
  }

  uint64_t start_ns = 0;
  uint64_t timeout_ns = 0;
  if (timeout != NULL) {
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
      return IpcChannelReadResult_error_body(
          IPC_ERR_SYSTEM, "system error: clock_gettime failed", error);
    }
    start_ns = ipc_timespec_to_nanos(&start_time);
    timeout_ns = ipc_timespec_to_nanos(timeout);
  }

  struct timespec delay = {.tv_sec = 0,
                           .tv_nsec = channel->config.start_sleep_ns};

  uint64_t prev_seen_offset = UINT64_MAX;
  bool prev_inited = false;
  size_t round_trips = 0;

  IpcEntry read_entry = {.offset = 0, .payload = NULL, .size = 0};

  for (;;) {
    IpcEntry peek_entry;
    const IpcBufferPeekResult peek_result =
        ipc_buffer_peek(channel->buffer, &peek_entry);
    if (IpcBufferPeekResult_is_error(peek_result) && !_is_retry_status(peek_result.ipc_status)) {
      free(read_entry.payload);
      error.offset = peek_entry.offset;
      return IpcChannelReadResult_error_body(peek_result.ipc_status, peek_result.error.detail,
        error);
    }

    if (timeout != NULL) {
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
    } else {
      const IpcStatus status = peek_result.ipc_status;
      const uint64_t curr_offset = peek_entry.offset;

      if (prev_inited && curr_offset == prev_seen_offset && status != IPC_OK) {
        round_trips++;
      } else {
        round_trips = 0;
      }
      prev_seen_offset = curr_offset;
      prev_inited = true;

      if (round_trips == channel->config.max_round_trips) {
        free(read_entry.payload);
        error.offset = peek_entry.offset;
        return IpcChannelReadResult_error_body(
            IPC_ERR_RETRY_LIMIT, "limit is reached: retry limit reached", error);
      }
    }

    if (_is_retry_status(peek_result.ipc_status)) {
      if (!_sleep_and_expand_delay(&delay, channel->config.max_sleep_ns)) {
        free(read_entry.payload);
        error.offset = peek_entry.offset;
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: nanosleep failed", error);
      }
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

static IpcChannelReadResult _try_read(IpcChannel *channel, IpcEntry *dest) {
  IpcChannelReadError error = {.offset = 0};

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

static inline bool _sleep_and_expand_delay(struct timespec *delay,
                                           const long max_sleep_ns) {
  if (nanosleep(delay, NULL) != 0) {
    return false;
  }

  if (delay->tv_nsec < max_sleep_ns) {
    delay->tv_nsec *= WAIT_EXPAND_FACTOR;
    if (delay->tv_nsec > max_sleep_ns) {
      delay->tv_nsec = max_sleep_ns;
    }
  }

  return true;
}

static inline bool _is_error_status(const IpcStatus status) {
  return !_is_retry_status(status) && status != IPC_OK;
}

static inline bool _is_retry_status(const IpcStatus status) {
  return status == IPC_ERR_NOT_READY || status == IPC_EMPTY ||
         status == IPC_ERR_LOCKED || status == IPC_ERR_CORRUPTED;
}

static inline bool _is_valid_config(const IpcChannelConfiguration config) {
  bool valid = (config.start_sleep_ns > 0) && (config.max_round_trips > 0) &&
               (config.max_sleep_ns > 0) &&
               (config.max_sleep_ns >= config.start_sleep_ns);
  return valid;
}

static inline bool _is_timeout_valid(const struct timespec *timeout) {
  const bool is_valid =
      (timeout == NULL) || ((timeout->tv_nsec >= 0) && (timeout->tv_sec >= 0));
  return is_valid;
}
