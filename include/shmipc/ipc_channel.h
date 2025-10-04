#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <time.h>

SHMIPC_BEGIN_DECLS

typedef struct IpcChannel IpcChannel;

typedef struct IpcChannelConfiguration {
  uint32_t max_round_trips;
  long start_sleep_ns;
  long max_sleep_ns;
} IpcChannelConfiguration;
SHMIPC_API uint64_t ipc_channel_align_size(size_t size);

typedef struct IpcChannelOpenError {
  size_t requested_size;
  size_t min_size;
  IpcChannelConfiguration config;
} IpcChannelOpenError;
IPC_RESULT(IpcChannelResult, IpcChannel *, IpcChannelOpenError)
SHMIPC_API IpcChannelResult ipc_channel_create(
    void *mem, const size_t size, const IpcChannelConfiguration config);

typedef struct IpcChannelConnectError {
  size_t min_size;
  IpcChannelConfiguration config;
} IpcChannelConnectError;
IPC_RESULT(IpcChannelConnectResult, IpcChannel *, IpcChannelConnectError)
SHMIPC_API IpcChannelConnectResult
ipc_channel_connect(void *mem, const IpcChannelConfiguration config);

typedef struct IpcChannelDestroyError {
  bool _unit;
} IpcChannelDestroyError;
IPC_RESULT_UNIT(IpcChannelDestroyResult, IpcChannelDestroyError)
SHMIPC_API IpcChannelDestroyResult ipc_channel_destroy(IpcChannel *channel);

typedef struct IpcChannelWriteError {
  IpcEntryId entry_id;
  size_t requested_size;
  size_t available_contiguous;
  size_t buffer_size;
} IpcChannelWriteError;
IPC_RESULT_UNIT(IpcChannelWriteResult, IpcChannelWriteError)
SHMIPC_API IpcChannelWriteResult ipc_channel_write(IpcChannel *channel,
                                                   const void *data,
                                                   const size_t size);

typedef struct IpcChannelReadError {
  IpcEntryId entry_id;
} IpcChannelReadError;
IPC_RESULT_UNIT(IpcChannelReadResult, IpcChannelReadError)
SHMIPC_API IpcChannelReadResult ipc_channel_read(IpcChannel *channel,
                                                 IpcEntry *dest);

typedef struct IpcChannelReadWithTimeoutError {
  IpcEntryId entry_id;
  struct timespec timeout_used;
} IpcChannelReadWithTimeoutError;
IPC_RESULT_UNIT(IpcChannelReadWithTimeoutResult, IpcChannelReadWithTimeoutError)
SHMIPC_API IpcChannelReadWithTimeoutResult ipc_channel_read_with_timeout(
    IpcChannel *channel, IpcEntry *dest, const struct timespec *timeout);

typedef struct IpcChannelTryReadError {
  IpcEntryId entry_id;
} IpcChannelTryReadError;
IPC_RESULT_UNIT(IpcChannelTryReadResult, IpcChannelTryReadError)
SHMIPC_API IpcChannelTryReadResult ipc_channel_try_read(IpcChannel *channel,
                                                        IpcEntry *dest);

typedef struct IpcChannelPeekError {
  IpcEntryId entry_id;
} IpcChannelPeekError;
IPC_RESULT_UNIT(IpcChannelPeekResult, IpcChannelPeekError)
SHMIPC_API IpcChannelPeekResult ipc_channel_peek(const IpcChannel *channel,
                                                 IpcEntry *dest);

typedef struct IpcChannelSkipError {
  IpcEntryId entry_id;
} IpcChannelSkipError;
IPC_RESULT(IpcChannelSkipResult, IpcEntryId, IpcChannelSkipError)
SHMIPC_API IpcChannelSkipResult ipc_channel_skip(IpcChannel *channel,
                                                 const IpcEntryId id);

typedef struct IpcChannelSkipForceError {
  bool _unit;
} IpcChannelSkipForceError;
IPC_RESULT(IpcChannelSkipForceResult, IpcEntryId, IpcChannelSkipForceError)
SHMIPC_API IpcChannelSkipForceResult
ipc_channel_skip_force(IpcChannel *channel);

SHMIPC_END_DECLS
