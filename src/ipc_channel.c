#include "ipc_utils.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_channel.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/signal.h>

#define WAIT_EXPAND_FACTOR 2

struct IpcChannel {
  IpcBuffer *buffer;
  IpcChannelConfiguration config;
};

IpcTransaction _read(IpcChannel *, IpcEntry *, const struct timespec *);
IpcTransaction _try_read(IpcChannel *, IpcEntry *);
bool _is_error_status(const IpcStatus);
bool _is_retry_status(const IpcStatus);
bool _sleep_and_expand_delay(struct timespec *, const long);
bool _is_valid_config(const IpcChannelConfiguration);
bool _is_timeout_valid(const struct timespec *);

inline uint64_t ipc_channel_allign_size(uint64_t size) {
  return ipc_buffer_allign_size(size);
}

IpcChannel *ipc_channel_create(void *mem, const uint64_t size,
                               const IpcChannelConfiguration config) {
  if (mem == NULL || size == 0 || !_is_valid_config(config)) {
    return NULL;
  }

  IpcChannel *channel = malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    return NULL;
  }

  channel->buffer = ipc_buffer_create(mem, size);
  if (channel->buffer == NULL) {
    free(channel);
    return NULL;
  }
  channel->config = config;

  return channel;
}

IpcChannel *ipc_channel_connect(void *mem,
                                const IpcChannelConfiguration config) {
  if (mem == NULL || !_is_valid_config(config)) {
    return NULL;
  }

  IpcChannel *channel = malloc(sizeof(IpcChannel));
  if (channel == NULL) {
    return NULL;
  }

  channel->buffer = ipc_buffer_attach(mem);
  if (channel->buffer == NULL) {
    free(channel);
    return NULL;
  }
  channel->config = config;

  return channel;
}

IpcStatus ipc_channel_destroy(IpcChannel *channel) {
  if (channel == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  free(channel->buffer);
  free(channel);

  return IPC_OK;
}

IpcStatus ipc_channel_write(IpcChannel *channel, const void *data,
                            const uint64_t size) {
  if (channel == NULL || data == NULL || size == 0) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  if (channel->buffer == NULL) {
    return IPC_ERR;
  }

  return ipc_buffer_write(channel->buffer, data, size);
}

IpcTransaction ipc_channel_try_read(IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL || dest == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
  }

  IpcEntry read_entry = {.payload = NULL, .size = 0};
  const IpcTransaction tx = _try_read(channel, &read_entry);
  if (tx.status != IPC_OK) {
    free(read_entry.payload);
    return tx;
  }

  dest->payload = read_entry.payload;
  dest->size = read_entry.size;

  return tx;
}

IpcTransaction ipc_channel_read(IpcChannel *channel, IpcEntry *dest) {
  return _read(channel, dest, NULL);
}

IpcTransaction ipc_channel_read_with_timeout(IpcChannel *channel,
                                             IpcEntry *dest,
                                             const struct timespec *timeout) {
  return _read(channel, dest, timeout);
}

IpcTransaction ipc_channel_peek(const IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL || dest == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
  }

  return ipc_buffer_peek(channel->buffer, dest);
}

IpcTransaction ipc_channel_skip(IpcChannel *channel, const IpcEntryId id) {
  if (channel == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
  }

  return ipc_buffer_skip(channel->buffer, id);
}

IpcTransaction ipc_channel_skip_force(IpcChannel *channel) {
  if (channel == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
  }

  return ipc_buffer_skip_force(channel->buffer);
}

IpcTransaction _read(IpcChannel *channel, IpcEntry *dest,
                     const struct timespec *timeout) {
  if (channel == NULL || dest == NULL || !_is_timeout_valid(timeout)) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
  }

  uint64_t start_ns;
  uint64_t timeout_ns;
  if (timeout != NULL) {
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
      return ipc_create_transaction(0, IPC_ERR);
    }

    start_ns = ipc_timespec_to_nanos(&start_time);
    timeout_ns = ipc_timespec_to_nanos(timeout);
  }

  struct timespec delay = {.tv_sec = 0,
                           .tv_nsec = channel->config.start_sleep_ns};

  IpcTransaction curr_tx;
  IpcTransaction prev_tx;
  size_t round_trips = 0;

  IpcEntry read_entry = {.payload = NULL, .size = 0};
  do {
    IpcEntry peek_entry;
    curr_tx = ipc_buffer_peek(channel->buffer, &peek_entry);
    if (_is_error_status(curr_tx.status)) {
      free(read_entry.payload);
      return curr_tx;
    }

    if (timeout != NULL) {
      struct timespec curr_time;
      if (clock_gettime(CLOCK_MONOTONIC, &curr_time) != 0) {
        free(read_entry.payload);
        return ipc_create_transaction(0, IPC_ERR);
      }

      const uint64_t curr_ns = ipc_timespec_to_nanos(&curr_time);
      if (curr_ns - start_ns >= timeout_ns) {
        free(read_entry.payload);
        return ipc_create_transaction(curr_tx.entry_id, IPC_TIMEOUT);
      }
    }

    if (timeout == NULL) {
      if (prev_tx.entry_id == curr_tx.entry_id && curr_tx.status != IPC_OK) {
        round_trips++;
      } else {
        round_trips = 0;
      }
      prev_tx = curr_tx;
    }

    if (timeout == NULL && round_trips == channel->config.max_round_trips) {
      free(read_entry.payload);
      return ipc_create_transaction(curr_tx.entry_id, IPC_REACHED_RETRY_LIMIT);
    }

    if (_is_retry_status(curr_tx.status)) {
      if (!_sleep_and_expand_delay(&delay, channel->config.max_sleep_ns)) {
        free(read_entry.payload);
        return ipc_create_transaction(0, IPC_ERR);
      }
      continue;
    }

    curr_tx = _try_read(channel, &read_entry);
    if (_is_error_status(curr_tx.status)) {
      free(read_entry.payload);
      return curr_tx;
    }
  } while (curr_tx.status != IPC_OK);

  dest->payload = read_entry.payload;
  dest->size = read_entry.size;

  return curr_tx;
}

IpcTransaction _try_read(IpcChannel *channel, IpcEntry *read_entry) {
  if (channel == NULL || read_entry == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
  }

  IpcTransaction curr_tx;
  do {
    IpcEntry peek_entry;
    curr_tx = ipc_buffer_peek(channel->buffer, &peek_entry);
    if (_is_error_status(curr_tx.status)) {
      return curr_tx;
    }

    if (curr_tx.status != IPC_OK) {
      return curr_tx;
    }

    if (read_entry->payload == NULL) {
      read_entry->payload = malloc(peek_entry.size);
      if (read_entry->payload == NULL) {
        return ipc_create_transaction(curr_tx.entry_id, IPC_ERR_ALLOCATION);
      }
      read_entry->size = peek_entry.size;
    } else if (read_entry->size < peek_entry.size) {
      void *new_buf = realloc(read_entry->payload, peek_entry.size);
      if (new_buf == NULL) {
        return ipc_create_transaction(curr_tx.entry_id, IPC_ERR_ALLOCATION);
      }

      read_entry->payload = new_buf;
      read_entry->size = peek_entry.size;
    }

    curr_tx = ipc_buffer_read(channel->buffer, read_entry);
    if (curr_tx.status == IPC_ERR_TOO_SMALL) {
      continue;
    }

    if (curr_tx.status != IPC_OK) {
      return curr_tx;
    }
  } while (curr_tx.status != IPC_OK);

  return curr_tx;
}

inline bool _is_error_status(const IpcStatus status) {
  return !_is_retry_status(status) && status != IPC_OK;
}

inline bool _is_retry_status(const IpcStatus status) {
  return status == IPC_NOT_READY || status == IPC_EMPTY ||
         status == IPC_LOCKED || status == IPC_CORRUPTED;
}

inline bool _sleep_and_expand_delay(struct timespec *delay,
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

inline bool _is_valid_config(const IpcChannelConfiguration config) {
  return config.start_sleep_ns > 0 && config.max_round_trips > 0 &&
         config.max_sleep_ns > 0 &&
         config.max_sleep_ns >= config.start_sleep_ns;
}

inline bool _is_timeout_valid(const struct timespec *timeout) {
  return timeout == NULL || (timeout->tv_nsec >= 0 && timeout->tv_sec >= 0);
}
