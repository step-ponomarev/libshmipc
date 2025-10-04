#include "ipc_utils.h"
#include "shmipc/ipc_buffer.h"
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

  if (mem == NULL) {
    IpcChannelOpenError body = {
        .requested_size = size, .min_size = min_total, .config = config};
    return IpcChannelResult_error_body(IPC_ERR_INVALID_ARGUMENT,
                                       "invalid argument: mem is NULL", body);
  }

  if (size == 0) {
    IpcChannelOpenError body = {
        .requested_size = size, .min_size = min_total, .config = config};
    return IpcChannelResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer size is 0", body);
  }

  if (!_is_valid_config(config)) {
    IpcChannelOpenError body = {
        .requested_size = size, .min_size = min_total, .config = config};
    return IpcChannelResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: valid config must be {start_sleep_ns > 0 && "
        "max_round_trips > 0 && max_sleep_ns > 0 && max_sleep_ns >= "
        "start_sleep_ns}",
        body);
  }

  IpcChannel *channel = (IpcChannel *)malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    IpcChannelOpenError body = {
        .requested_size = size, .min_size = min_total, .config = config};
    return IpcChannelResult_error_body(
        IPC_ERR_SYSTEM, "system error: channel allocation failed", body);
  }

  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem, (size_t)size);
  if (IpcBufferCreateResult_is_error(buffer_result)) {
    IpcChannelOpenError body = {
        .requested_size = size, .min_size = min_total, .config = config};
    free(channel);
    return IpcChannelResult_error_body(buffer_result.ipc_status,
                                       buffer_result.error.detail, body);
  }

  channel->buffer = buffer_result.result;
  channel->config = config;

  return IpcChannelResult_ok(IPC_OK, channel);
}

IpcChannelConnectResult ipc_channel_connect(void *mem,
                                            const IpcChannelConfiguration config) {
  const size_t min_total = ipc_buffer_align_size(2);

  if (mem == NULL) {
    IpcChannelConnectError body = {
        .min_size = min_total, .config = config};
    return IpcChannelConnectResult_error_body(IPC_ERR_INVALID_ARGUMENT,
                                              "invalid argument: mem is NULL", body);
  }

  if (!_is_valid_config(config)) {
    IpcChannelConnectError body = {
        .min_size = min_total, .config = config};
    return IpcChannelConnectResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: valid config must be {start_sleep_ns > 0 && "
        "max_round_trips > 0 && max_sleep_ns > 0 && max_sleep_ns >= "
        "start_sleep_ns}",
        body);
  }

  IpcChannel *channel = (IpcChannel *)malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    IpcChannelConnectError body = {
        .min_size = min_total, .config = config};
    return IpcChannelConnectResult_error_body(
        IPC_ERR_SYSTEM, "system error: channel allocation failed", body);
  }

  const IpcBufferAttachResult buffer_result = ipc_buffer_attach(mem);
  if (IpcBufferAttachResult_is_error(buffer_result)) {
    IpcChannelConnectError body = {
        .min_size = min_total, .config = config};
    free(channel);
    return IpcChannelConnectResult_error_body(buffer_result.ipc_status,
                                              buffer_result.error.detail, body);
  }

  channel->buffer = buffer_result.result;
  channel->config = config;

  return IpcChannelConnectResult_ok(IPC_OK, channel);
}

IpcChannelDestroyResult ipc_channel_destroy(IpcChannel *channel) {
  if (channel == NULL) {
    IpcChannelDestroyError body = {.had_buffer = false};
    return IpcChannelDestroyResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelDestroyError body = {.had_buffer = (channel->buffer != NULL)};
    return IpcChannelDestroyResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  free(channel->buffer);
  free(channel);
  return IpcChannelDestroyResult_ok(IPC_OK);
}

IpcChannelWriteResult ipc_channel_write(IpcChannel *channel, const void *data,
                                        const size_t size) {
  IpcChannelWriteError werr = {.entry_id = 0,
                               .requested_size = (size_t)size,
                               .available_contiguous = 0,
                               .buffer_size = 0};

  if (channel == NULL) {
    return IpcChannelWriteResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", werr);
  }

  if (channel->buffer == NULL) {
    return IpcChannelWriteResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", werr);
  }

  const IpcBufferWriteResult wr =
      ipc_buffer_write(channel->buffer, data, (size_t)size);
  if (IpcBufferWriteResult_is_error(wr)) {
    if (IpcBufferWriteResult_is_error_has_body(wr.error)) {
      const IpcBufferWriteError b = wr.error.body;
      werr.entry_id = b.entry_id;
      werr.requested_size = b.requested_size;
      werr.available_contiguous = b.available_contiguous;
      werr.buffer_size = b.buffer_size;
    }
    return IpcChannelWriteResult_error_body(wr.ipc_status, wr.error.detail,
                                            werr);
  }

  return IpcChannelWriteResult_ok(wr.ipc_status);
}

IpcChannelTryReadResult ipc_channel_try_read(IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL) {
    IpcChannelTryReadError body = {.entry_id = 0};
    return IpcChannelTryReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelTryReadError body = {.entry_id = 0};
    return IpcChannelTryReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  if (dest == NULL) {
    IpcChannelTryReadError body = {.entry_id = 0};
    return IpcChannelTryReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", body);
  }

  IpcEntry read_entry = {.id = 0, .payload = NULL, .size = 0};
  const IpcChannelReadResult rx = _try_read(channel, &read_entry);
  if (rx.ipc_status != IPC_OK) {
    free(read_entry.payload);
    // Конвертируем IpcChannelReadResult в IpcChannelTryReadResult
    if (IpcChannelReadResult_is_error(rx)) {
      IpcChannelTryReadError body = {.entry_id = rx.error.body.entry_id};
      return IpcChannelTryReadResult_error_body(rx.ipc_status, rx.error.detail, body);
    }
    return IpcChannelTryReadResult_ok(rx.ipc_status);
  }

  dest->payload = read_entry.payload;
  dest->size = read_entry.size;
  dest->id = read_entry.id;

  return IpcChannelTryReadResult_ok(IPC_OK);
}

IpcChannelReadResult ipc_channel_read(IpcChannel *channel, IpcEntry *dest) {
  return _read(channel, dest, NULL);
}

IpcChannelReadWithTimeoutResult
ipc_channel_read_with_timeout(IpcChannel *channel, IpcEntry *dest,
                              const struct timespec *timeout) {
  const IpcChannelReadResult rx = _read(channel, dest, timeout);
  // Конвертируем IpcChannelReadResult в IpcChannelReadWithTimeoutResult
  if (IpcChannelReadResult_is_error(rx)) {
    IpcChannelReadWithTimeoutError body = {
      .entry_id = rx.error.body.entry_id,
      .timeout_used = timeout ? *timeout : (struct timespec){0, 0}
    };
    return IpcChannelReadWithTimeoutResult_error_body(rx.ipc_status, rx.error.detail, body);
  }
  return IpcChannelReadWithTimeoutResult_ok(rx.ipc_status);
}

IpcChannelPeekResult ipc_channel_peek(const IpcChannel *channel,
                                      IpcEntry *dest) {
  if (channel == NULL) {
    IpcChannelPeekError body = {.entry_id = 0};
    return IpcChannelPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelPeekError body = {.entry_id = 0};
    return IpcChannelPeekResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  if (dest == NULL) {
    IpcChannelPeekError body = {.entry_id = 0};
    return IpcChannelPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", body);
  }

  const IpcBufferPeekResult pr = ipc_buffer_peek(channel->buffer, dest);
  if (IpcBufferPeekResult_is_error(pr)) {
    IpcChannelPeekError body = {.entry_id = dest->id};
    return IpcChannelPeekResult_error_body(pr.ipc_status, pr.error.detail,
                                           body);
  }
  return IpcChannelPeekResult_ok(pr.ipc_status);
}

IpcChannelSkipResult ipc_channel_skip(IpcChannel *channel,
                                      const IpcEntryId id) {
  if (channel == NULL) {
    IpcChannelSkipError body = {.entry_id = id};
    return IpcChannelSkipResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelSkipError body = {.entry_id = id};
    return IpcChannelSkipResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  const IpcBufferSkipResult sr = ipc_buffer_skip(channel->buffer, id);
  if (IpcBufferSkipResult_is_error(sr)) {
    IpcChannelSkipError body = {.entry_id = id};
    return IpcChannelSkipResult_error_body(sr.ipc_status, sr.error.detail,
                                           body);
  }
  return IpcChannelSkipResult_ok(sr.ipc_status, sr.result);
}

IpcChannelSkipForceResult ipc_channel_skip_force(IpcChannel *channel) {
  if (channel == NULL) {
    IpcChannelSkipForceError body = {.entry_id = 0};
    return IpcChannelSkipForceResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelSkipForceError body = {.entry_id = 0};
    return IpcChannelSkipForceResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  const IpcBufferSkipForceResult sfr = ipc_buffer_skip_force(channel->buffer);
  if (IpcBufferSkipForceResult_is_error(sfr)) {
    IpcChannelSkipForceError body = {.entry_id = 0};
    return IpcChannelSkipForceResult_error_body(sfr.ipc_status,
                                                sfr.error.detail, body);
  }
  return IpcChannelSkipForceResult_ok(sfr.ipc_status, sfr.result);
}

/* ---------------- Internals ---------------- */

static IpcChannelReadResult _read(IpcChannel *channel, IpcEntry *dest,
                                  const struct timespec *timeout) {
  if (channel == NULL) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  if (dest == NULL) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", body);
  }

  if (!_is_timeout_valid(timeout)) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: valid timeout must be "
        "{timeout == NULL || (timeout->tv_nsec >= 0 && timeout->tv_sec >= 0)}",
        body);
  }

  uint64_t start_ns = 0;
  uint64_t timeout_ns = 0;
  if (timeout != NULL) {
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
      IpcChannelReadError body = {.entry_id = 0};
      return IpcChannelReadResult_error_body(
          IPC_ERR_SYSTEM, "system error: clock_gettime failed", body);
    }
    start_ns = ipc_timespec_to_nanos(&start_time);
    timeout_ns = ipc_timespec_to_nanos(timeout);
  }

  struct timespec delay = {.tv_sec = 0,
                           .tv_nsec = channel->config.start_sleep_ns};

  uint64_t prev_seen_id = UINT64_MAX;
  bool prev_inited = false;
  size_t round_trips = 0;

  IpcEntry read_entry = {.id = 0, .payload = NULL, .size = 0};

  for (;;) {
    IpcEntry peek_entry;
    const IpcBufferPeekResult pk =
        ipc_buffer_peek(channel->buffer, &peek_entry);
    if (IpcBufferPeekResult_is_error(pk) && !_is_retry_status(pk.ipc_status)) {
      free(read_entry.payload);
      IpcChannelReadError body = {.entry_id = peek_entry.id};
      return IpcChannelReadResult_error_body(pk.ipc_status, pk.error.detail,
                                             body);
    }

    if (timeout != NULL) {
      struct timespec curr_time;
      if (clock_gettime(CLOCK_MONOTONIC, &curr_time) != 0) {
        free(read_entry.payload);
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: clock_gettime failed", body);
      }

      const uint64_t curr_ns = ipc_timespec_to_nanos(&curr_time);
      if (curr_ns - start_ns >= timeout_ns) {
        free(read_entry.payload);
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(IPC_ERR_TIMEOUT,
                                               "timeout: read timed out", body);
      }
    } else {
      const IpcStatus st = pk.ipc_status;
      const uint64_t curr_id = peek_entry.id;

      if (prev_inited && curr_id == prev_seen_id && st != IPC_OK) {
        round_trips++;
      } else {
        round_trips = 0;
      }
      prev_seen_id = curr_id;
      prev_inited = true;

      if (round_trips == channel->config.max_round_trips) {
        free(read_entry.payload);
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(
            IPC_ERR_RETRY_LIMIT, "limit is reached: retry limit reached", body);
      }
    }

    if (_is_retry_status(pk.ipc_status)) {
      if (!_sleep_and_expand_delay(&delay, channel->config.max_sleep_ns)) {
        free(read_entry.payload);
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: nanosleep failed", body);
      }
      continue;
    }

    const IpcChannelReadResult rx = _try_read(channel, &read_entry);
    if (_is_error_status(rx.ipc_status)) {
      free(read_entry.payload);
      return rx;
    }

    if (rx.ipc_status == IPC_OK) {
      dest->payload = read_entry.payload;
      dest->size = read_entry.size;
      dest->id = read_entry.id;
      return rx;
    }
    /* иначе повторить цикл */
  }
}

static IpcChannelReadResult _try_read(IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: channel is NULL", body);
  }

  if (channel->buffer == NULL) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: channel->buffer is NULL", body);
  }

  if (dest == NULL) {
    IpcChannelReadError body = {.entry_id = 0};
    return IpcChannelReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: data is NULL", body);
  }

  for (;;) {
    IpcEntry peek_entry;
    const IpcBufferPeekResult pr =
        ipc_buffer_peek(channel->buffer, &peek_entry);
    if (pr.ipc_status != IPC_OK) {
      if (IpcBufferPeekResult_is_error(pr)) {
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(pr.ipc_status, pr.error.detail,
                                               body);
      }
      /* EMPTY/NOT_READY/LOCKED — просто возвращаем статус как ok-result */
      return IpcChannelReadResult_ok(pr.ipc_status);
    }

    /* ensure capacity */
    if (dest->payload == NULL) {
      dest->payload = malloc(peek_entry.size);
      if (dest->payload == NULL) {
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: allocation failed", body);
      }
      dest->size = peek_entry.size;
    } else if (dest->size < peek_entry.size) {
      void *new_buf = realloc(dest->payload, peek_entry.size);
      if (new_buf == NULL) {
        IpcChannelReadError body = {.entry_id = peek_entry.id};
        return IpcChannelReadResult_error_body(
            IPC_ERR_SYSTEM, "system error: allocation failed", body);
      }
      dest->payload = new_buf;
      dest->size = peek_entry.size;
    }

    const IpcBufferReadResult rr = ipc_buffer_read(channel->buffer, dest);
    if (IpcBufferReadResult_is_error(rr) &&
        rr.ipc_status == IPC_ERR_TOO_SMALL) {
      /* гонка: payload вырос между peek и read; попробуем ещё раз, расширив
       * буфер выше */
      continue;
    }

    if (rr.ipc_status != IPC_OK) {
      if (IpcBufferReadResult_is_error(rr)) {
        IpcChannelReadError body = {.entry_id = dest->id};
        return IpcChannelReadResult_error_body(rr.ipc_status, rr.error.detail,
                                               body);
      }
      return IpcChannelReadResult_ok(rr.ipc_status);
    }

    /* success */
    return IpcChannelReadResult_ok(IPC_OK);
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
