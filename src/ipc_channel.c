#include "ipc_utils.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_common.h"
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_channel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syslimits.h>
#include <time.h>

#define WAIT_EXPAND_FACTOR 2

struct IpcChannel {
  IpcBuffer *buffer;
  IpcChannelConfiguration config;
};

bool _is_error_status(const IpcStatus);
bool _is_retry_status(const IpcStatus);
bool _sleep_and_expand_delay(struct timespec, const long);
bool _is_valid_config(const IpcChannelConfiguration);

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

// запись никогда не блокирует
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

// блокируещее чтение
// if loong read and curropted should return to client
IpcTransaction ipc_channel_read(IpcChannel *channel, IpcEntry *dest) {
  if (channel == NULL || dest == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  if (channel->buffer == NULL) {
    return ipc_create_transaction(0, IPC_ERR);
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

    if (prev_tx.entry_id == curr_tx.entry_id &&
        curr_tx.status == IPC_NOT_READY) {
      round_trips++;
    } else {
      round_trips = 0;
    }
    prev_tx = curr_tx;

    if (round_trips == channel->config.max_round_trips) {
      free(read_entry.payload);
      return ipc_create_transaction(curr_tx.entry_id, IPC_REACHED_RETRY_LIMIT);
    }

    if (_is_retry_status(curr_tx.status)) {
      if (!_sleep_and_expand_delay(delay, channel->config.max_sleep_ns)) {
        free(read_entry.payload);
        return ipc_create_transaction(0, IPC_ERR);
      }
      continue;
    }

    if (read_entry.payload == NULL) {
      read_entry.payload = malloc(peek_entry.size);
      read_entry.size = peek_entry.size;
    } else if (read_entry.size < peek_entry.size) {
      void *new_buf = realloc(read_entry.payload, peek_entry.size);
      if (new_buf == NULL) {
        free(read_entry.payload);
        return ipc_create_transaction(curr_tx.entry_id, IPC_ERR_ALLOCATION);
      }

      read_entry.payload = new_buf;
      read_entry.size = peek_entry.size;
    }

    if (read_entry.payload == NULL) {
      return ipc_create_transaction(curr_tx.entry_id, IPC_ERR_ALLOCATION);
    }

    curr_tx = ipc_buffer_read(channel->buffer, &read_entry);
    if (curr_tx.status == IPC_ERR_TOO_SMALL) {
      continue;
    }

    if (_is_error_status(curr_tx.status)) {
      free(read_entry.payload);
      return curr_tx;
    }
  } while (curr_tx.status != IPC_OK);

  dest->payload = read_entry.payload;
  dest->size = read_entry.size;

  return curr_tx;
}
// IpcStatus ipc_channel_read_with_timeout(IpcChannel *, IpcEntry *, time_t);
//

inline bool _is_error_status(const IpcStatus status) {
  return !_is_retry_status(status) && status != IPC_OK;
}

inline bool _is_retry_status(const IpcStatus status) {
  return status == IPC_NOT_READY || status == IPC_EMPTY;
}

inline bool _sleep_and_expand_delay(struct timespec delay,
                                    const long max_sleep_ns) {
  if (nanosleep(&delay, NULL) != 0) {
    return false;
  }

  if (delay.tv_nsec < max_sleep_ns) {
    delay.tv_nsec *= WAIT_EXPAND_FACTOR;
    if (delay.tv_nsec > max_sleep_ns) {
      delay.tv_nsec = max_sleep_ns;
    }
  }

  return true;
}

inline bool _is_valid_config(const IpcChannelConfiguration config) {
  return config.start_sleep_ns > 0 && config.max_round_trips > 0 &&
         config.max_sleep_ns > 0;
}
