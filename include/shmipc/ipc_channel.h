#ifndef IPC_CHANNEL_H
#define IPC_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shmipc/ipc_common.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct IpcChannel IpcChannel;

typedef struct IpcChannelConfiguration {
  uint32_t max_round_trips;
  long start_sleep_ns;
  long max_sleep_ns;
} IpcChannelConfiguration;

uint64_t ipc_channel_allign_size(uint64_t);

IpcChannel *ipc_channel_create(void *, const uint64_t,
                               const IpcChannelConfiguration);
IpcChannel *ipc_channel_connect(void *, const IpcChannelConfiguration);
IpcStatus ipc_channel_destroy(IpcChannel *);

// запись никогда не блокирует
IpcStatus ipc_channel_write(IpcChannel *, const void *, const uint64_t);

// блокируещее чтение
IpcTransaction ipc_channel_read(IpcChannel *, IpcEntry *);

// Чтение с таймаутом
IpcTransaction ipc_channel_read_with_timeout(IpcChannel *, IpcEntry *,
                                             const struct timespec *);
IpcTransaction ipc_channel_try_read(IpcChannel *, IpcEntry *);

IpcTransaction ipc_channel_skip(IpcChannel *, const IpcEntryId);
IpcTransaction ipc_channel_skip_force(IpcChannel *);

#ifdef __cplusplus
}
#endif

#endif
