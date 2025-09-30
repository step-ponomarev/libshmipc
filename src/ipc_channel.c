#include "ipc_utils.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_channel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define WAIT_EXPAND_FACTOR 2

struct IpcChannel {
  IpcBuffer *buffer;
  IpcChannelConfiguration config;
};

IpcTransactionResult _read(IpcChannel *, IpcEntry *, const struct timespec *);
IpcTransactionResult _try_read(IpcChannel *, IpcEntry *);
BooleanResult _sleep_and_expand_delay(struct timespec *, const long);
BooleanResult _is_valid_config(const IpcChannelConfiguration);
BooleanResult _is_timeout_valid(const struct timespec *);
bool _is_error_status(const IpcStatus);
bool _is_retry_status(const IpcStatus);

inline uint64_t ipc_channel_allign_size(uint64_t size) {
  return ipc_buffer_allign_size(size);
}

IpcChannelResult ipc_channel_create(void *mem, const uint64_t size,
                                    const IpcChannelConfiguration config) {
  if (mem == NULL) {
    return IpcChannelResult_error(IPC_ERR_INVALID_ARGUMENT,
                                  "invalid argument: mem is NULL");
  }

  if (size == 0) {
    return IpcChannelResult_error(IPC_ERR_INVALID_ARGUMENT,

                                  "invalid argument: buffer size is 0");
  }

  const BooleanResult is_config_valid = _is_valid_config(config);
  if (BooleanResult_is_error(is_config_valid)) {
    return IpcChannelResult_error(IPC_ERR_INVALID_ARGUMENT,
                                  is_config_valid.error_detail);
  }

  IpcChannel *channel = malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    return IpcChannelResult_error(IPC_ERR_SYSTEM,
                                  "system error: channel allocation failed");
  }

  const IpcBufferResult buffer_result = ipc_buffer_create(mem, size);
  if (IpcBufferResult_is_error(buffer_result)) {
    free(channel);
    return IpcChannelResult_error(buffer_result.ipc_status,
                                  buffer_result.error_detail);
  }
  channel->buffer = (IpcBuffer *)buffer_result.result;
  channel->config = config;

  return IpcChannelResult_ok(IPC_OK, channel);
}

IpcChannelResult ipc_channel_connect(void *mem,
                                     const IpcChannelConfiguration config) {

  if (mem == NULL) {
    return IpcChannelResult_error(IPC_ERR_INVALID_ARGUMENT,
                                  "invalid argument: mem is NULL");
  }

  const BooleanResult is_config_valid = _is_valid_config(config);
  if (BooleanResult_is_error(is_config_valid)) {
    return IpcChannelResult_error(IPC_ERR_INVALID_ARGUMENT,
                                  is_config_valid.error_detail);
  }

  IpcChannel *channel = malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    return IpcChannelResult_error(IPC_ERR_SYSTEM,
                                  "system error: channel allocation failed");
  }

  const IpcBufferResult buffer_result = ipc_buffer_attach(mem);
  if (IpcBufferResult_is_error(buffer_result)) {
    free(channel);
    return IpcChannelResult_error(buffer_result.ipc_status,
                                  buffer_result.error_detail);
  }
  channel->buffer = (IpcBuffer *)buffer_result.result;
  channel->config = config;

  return IpcChannelResult_ok(IPC_OK, channel);
}

IpcStatusResult ipc_channel_destroy(IpcChannel *channel) {
  if (channel == NULL) {
    return IpcStatusResult_error(IPC_ERR_INVALID_ARGUMENT,
                                 "invalid argument: channel is NULL");
  }

  free(channel->buffer);
  free(channel);

  return IpcStatusResult_ok(IPC_OK, IPC_OK);
}

IpcStatusResult ipc_channel_write(IpcChannel *channel, const void *data,
                                  const uint64_t size) {
  if (channel == NULL) {
    return IpcStatusResult_error(IPC_ERR_INVALID_ARGUMENT,
                                 "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcStatusResult_error(IPC_ERR_ILLEGAL_STATE,
                                 "illegal state: channel->buffer is NULL");
  }

  return ipc_buffer_write(channel->buffer, data, size);
}

IpcTransactionResult ipc_channel_try_read(IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcTransactionResult_error(IPC_ERR_ILLEGAL_STATE,
                                      "illegal state: channel->buffer is NULL");
  }

  if (dest == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: dest is NULL");
  }

  IpcEntry read_entry = {.payload = NULL, .size = 0};
  const IpcTransactionResult tx = _try_read(channel, &read_entry);
  if (tx.ipc_status != IPC_OK) {
    free(read_entry.payload);
    return tx;
  }

  dest->payload = read_entry.payload;
  dest->size = read_entry.size;

  return tx;
}

IpcTransactionResult ipc_channel_read(IpcChannel *channel, IpcEntry *dest) {
  return _read(channel, dest, NULL);
}

IpcTransactionResult
ipc_channel_read_with_timeout(IpcChannel *channel, IpcEntry *dest,
                              const struct timespec *timeout) {
  return _read(channel, dest, timeout);
}

IpcTransactionResult ipc_channel_peek(const IpcChannel *channel,
                                      IpcEntry *dest) {
  if (channel == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcTransactionResult_error(IPC_ERR_ILLEGAL_STATE,
                                      "illegal state: channel->buffer is NULL");
  }

  if (dest == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: dest is NULL");
  }

  return ipc_buffer_peek(channel->buffer, dest);
}

IpcStatusResult ipc_channel_skip(IpcChannel *channel, const IpcEntryId id) {
  if (channel == NULL) {
    return IpcStatusResult_error(IPC_ERR_INVALID_ARGUMENT,
                                 "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcStatusResult_error(IPC_ERR_ILLEGAL_STATE,
                                 "illegal state: channel->buffer is NULL");
  }

  return ipc_buffer_skip(channel->buffer, id);
}

IpcTransactionResult ipc_channel_skip_force(IpcChannel *channel) {
  if (channel == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcTransactionResult_error(IPC_ERR_ILLEGAL_STATE,
                                      "illegal state: channel->buffer is NULL");
  }

  return ipc_buffer_skip_force(channel->buffer);
}

IpcTransactionResult _read(IpcChannel *channel, IpcEntry *dest,
                           const struct timespec *timeout) {
  if (channel == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcTransactionResult_error(IPC_ERR_ILLEGAL_STATE,
                                      "illegal state: channel->buffer is NULL");
  }

  if (dest == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: dest is NULL");
  }

  const BooleanResult is_timeout_valid = _is_timeout_valid(timeout);
  if (BooleanResult_is_error(is_timeout_valid)) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      is_timeout_valid.error_detail);
  }

  uint64_t start_ns;
  uint64_t timeout_ns;
  if (timeout != NULL) {
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
      return IpcTransactionResult_error(
          IPC_ERR_SYSTEM, "system error: clock_gettime is failed");
    }

    start_ns = ipc_timespec_to_nanos(&start_time);
    timeout_ns = ipc_timespec_to_nanos(timeout);
  }

  struct timespec delay = {.tv_sec = 0,
                           .tv_nsec = channel->config.start_sleep_ns};

  IpcTransactionResult curr_tx;
  IpcTransactionResult prev_tx;
  bool prev_inited = false;
  size_t round_trips = 0;

  IpcEntry read_entry = {.payload = NULL, .size = 0};
  do {
    IpcEntry peek_entry;
    curr_tx = ipc_buffer_peek(channel->buffer, &peek_entry);
    if (IpcTransactionResult_is_error(curr_tx) &&
        curr_tx.ipc_status != IPC_CORRUPTED) {
      free(read_entry.payload);
      return curr_tx;
    }

    if (timeout != NULL) {
      struct timespec curr_time;
      if (clock_gettime(CLOCK_MONOTONIC, &curr_time) != 0) {
        free(read_entry.payload);
        return IpcTransactionResult_error(
            IPC_ERR_SYSTEM, "system error: clock_gettime is failed");
      }

      const uint64_t curr_ns = ipc_timespec_to_nanos(&curr_time);
      if (curr_ns - start_ns >= timeout_ns) {
        free(read_entry.payload);
        return IpcTransactionResult_error(IPC_TIMEOUT,
                                          "timeout: read timed out");
      }
    } else {
      if (prev_inited && prev_tx.result == curr_tx.result &&
          curr_tx.ipc_status != IPC_OK) {
        round_trips++;
      } else {
        round_trips = 0;
      }
      prev_tx = curr_tx;
      prev_inited = true;

      if (round_trips == channel->config.max_round_trips) {
        free(read_entry.payload);
        return IpcTransactionResult_error(
            IPC_REACHED_RETRY_LIMIT, "limit is reached: retry limit reached");
      }
    }

    if (_is_retry_status(curr_tx.ipc_status)) {
      const BooleanResult sleep_result =
          _sleep_and_expand_delay(&delay, channel->config.max_sleep_ns);
      if (BooleanResult_is_error(sleep_result)) {
        free(read_entry.payload);
        return IpcTransactionResult_error(sleep_result.ipc_status,
                                          sleep_result.error_detail);
      }
      continue;
    }

    curr_tx = _try_read(channel, &read_entry);
    if (_is_error_status(curr_tx.ipc_status)) {
      free(read_entry.payload);
      return curr_tx;
    }
  } while (curr_tx.ipc_status != IPC_OK);

  dest->payload = read_entry.payload;
  dest->size = read_entry.size;

  return curr_tx;
}

IpcTransactionResult _try_read(IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: channel is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcTransactionResult_error(IPC_ERR_ILLEGAL_STATE,
                                      "illegal state: channel->buffer is NULL");
  }

  if (dest == NULL) {
    return IpcTransactionResult_error(IPC_ERR_INVALID_ARGUMENT,
                                      "invalid argument: data is NULL");
  }

  if (channel->buffer == NULL) {
    return IpcTransactionResult_error(IPC_ERR_ILLEGAL_STATE,
                                      "illegal state: channel->buffer is NULL");
  }

  IpcTransactionResult status_result;
  do {
    IpcEntry peek_entry;
    status_result = ipc_buffer_peek(channel->buffer, &peek_entry);
    if (status_result.ipc_status != IPC_OK) {
      return status_result;
    }

    if (dest->payload == NULL) {
      dest->payload = malloc(peek_entry.size);
      if (dest->payload == NULL) {
        return IpcTransactionResult_error(IPC_ERR_SYSTEM,
                                          "system error: allocation is failed");
      }
      dest->size = peek_entry.size;
    } else if (dest->size < peek_entry.size) {
      void *new_buf = realloc(dest->payload, peek_entry.size);
      if (new_buf == NULL) {
        return IpcTransactionResult_error(IPC_ERR_SYSTEM,
                                          "system error: allocation is failed");
      }

      dest->payload = new_buf;
      dest->size = peek_entry.size;
    }

    status_result = ipc_buffer_read(channel->buffer, dest);
    if (IpcTransactionResult_is_error(status_result) &&
        status_result.ipc_status == IPC_ERR_TOO_SMALL) {
      continue;
    }

    if (status_result.ipc_status != IPC_OK) {
      return status_result;
    }
  } while (status_result.ipc_status != IPC_OK);

  return status_result;
}

inline BooleanResult _sleep_and_expand_delay(struct timespec *delay,
                                             const long max_sleep_ns) {
  if (nanosleep(delay, NULL) != 0) {
    return BooleanResult_error(IPC_ERR_SYSTEM,
                               "system error: nanosleep is failed");
  }

  if (delay->tv_nsec < max_sleep_ns) {
    delay->tv_nsec *= WAIT_EXPAND_FACTOR;
    if (delay->tv_nsec > max_sleep_ns) {
      delay->tv_nsec = max_sleep_ns;
    }
  }

  return BooleanResult_ok(IPC_OK, true);
}

inline bool _is_error_status(const IpcStatus status) {
  return !_is_retry_status(status) && status != IPC_OK;
}

inline bool _is_retry_status(const IpcStatus status) {
  return status == IPC_NOT_READY || status == IPC_EMPTY ||
         status == IPC_LOCKED || status == IPC_CORRUPTED;
}

inline BooleanResult _is_valid_config(const IpcChannelConfiguration config) {
  bool valid = config.start_sleep_ns > 0 && config.max_round_trips > 0 &&
               config.max_sleep_ns > 0 &&
               config.max_sleep_ns >= config.start_sleep_ns;

  if (valid) {
    return BooleanResult_ok(IPC_OK, valid);
  }

  return BooleanResult_error(
      IPC_ERR_INVALID_ARGUMENT,
      "invalid argument: valid config must be {confistart_sleep_ns > 0 && "
      "max_round_trips > 0 &&  max_sleep_ns > 0 && max_sleep_ns >= "
      "start_sleep_ns}");
}

inline BooleanResult _is_timeout_valid(const struct timespec *timeout) {
  const bool is_vlid =
      timeout == NULL || (timeout->tv_nsec >= 0 && timeout->tv_sec >= 0);
  if (is_vlid) {
    return BooleanResult_ok(IPC_OK, is_vlid);
  }

  return BooleanResult_error(IPC_ERR_INVALID_ARGUMENT,
                             "invalid argument: valid timeout must be {timeout "
                             "== NULL || (timeout->tv_nsec "
                             ">= 0 && timeout->tv_sec >= 0");
}
