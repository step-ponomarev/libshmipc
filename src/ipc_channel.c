#include "shmipc/ipc_buffer.h"
#include <_time.h>
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_channel.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

struct IpcChannel {
  IpcBuffer *buffer;
};

IpcChannel *ipc_channel_create(uint8_t *mem, const uint64_t size) {
  if (mem == NULL || size == 0) {
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

  return channel;
}

IpcChannel *ipc_channel_connect(uint8_t *mem) {
  if (mem == NULL) {
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
IpcStatus ipc_channel_write(IpcChannel *channel, const uint8_t *data,
                            const uint64_t size) {
  if (channel == NULL || data == NULL || size == 0) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  if (channel->buffer == NULL) {
    return IPC_ERR;
  }

  return ipc_buffer_write(channel->buffer, data, size);
}

#define MAX_NANOS_MS 10000000
// блокируещее чтение
IpcStatus ipc_channel_read(IpcChannel *channel, IpcEntry *entry) {
  if (channel == NULL || entry == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  if (channel->buffer == NULL) {
    return IPC_ERR;
  }

  uint64_t delay_ns = 2;
  IpcStatus status;
  do {
    status = ipc_buffer_read(channel->buffer, entry);
    // TODO: если долго NOT_READY, то нужно это трекать
    if (status == IPC_NOT_READY || status == IPC_EMPTY) {
      struct timespec delay = {.tv_sec = 0, .tv_nsec = delay_ns};
      nanosleep(&delay, NULL);

      if (delay_ns < MAX_NANOS_MS) {
        delay_ns *= 2;
        delay_ns = delay_ns > MAX_NANOS_MS ? MAX_NANOS_MS : delay_ns;
      }
      continue;
    }

  } while (status != IPC_OK);

  return status;
}
// IpcStatus ipc_channel_read_with_timeout(IpcChannel *, IpcEntry *, time_t);
