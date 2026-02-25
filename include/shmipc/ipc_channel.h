#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_error.h>
#include <shmipc/ipc_status.h>
#include <shmipc/ipc_export.h>
#include <time.h>

SHMIPC_BEGIN_DECLS

typedef struct ipc_channel_t ipc_channel_t;

SHMIPC_API uint64_t ipc_channel_get_memory_overhead(void);
SHMIPC_API uint64_t ipc_channel_get_min_size(void);
SHMIPC_API uint64_t ipc_channel_suggest_size(size_t desired_capacity);
SHMIPC_API uint32_t ipc_channel_get_notify_signal(ipc_channel_t *channel);
SHMIPC_API bool ipc_channel_is_retry_status(const IpcStatus);

SHMIPC_API ipc_status_t ipc_channel_create(void *mem, size_t size, ipc_channel_t** out, ipc_error_t* err);
SHMIPC_API ipc_status_t ipc_channel_connect(void *mem, ipc_channel_t** out, ipc_error_t* err);

typedef struct IpcChannelDestroyError {
  bool _unit;
} IpcChannelDestroyError;
IPC_RESULT_UNIT(IpcChannelDestroyResult, IpcChannelDestroyError)
SHMIPC_API IpcChannelDestroyResult ipc_channel_destroy(ipc_channel_t *channel);

SHMIPC_API ipc_status_t ipc_channel_write(ipc_channel_t *channel, const void *data, size_t size, ipc_error_t* err);

typedef struct IpcChannelReadError {
  uint64_t offset;
  struct timespec timeout_used;
  int sys_errno;
} IpcChannelReadError;
IPC_RESULT_UNIT(IpcChannelReadResult, IpcChannelReadError)
SHMIPC_API IpcChannelReadResult ipc_channel_read(
    ipc_channel_t *channel, ipc_entry_t *dest, const struct timespec *timeout);

typedef struct IpcChannelTryReadError {
  uint64_t offset;
} IpcChannelTryReadError;
IPC_RESULT_UNIT(IpcChannelTryReadResult, IpcChannelTryReadError)
SHMIPC_API IpcChannelTryReadResult ipc_channel_try_read(ipc_channel_t *channel,
                                                        ipc_entry_t *dest);

typedef struct IpcChannelPeekError {
  uint64_t offset;
} IpcChannelPeekError;
IPC_RESULT_UNIT(IpcChannelPeekResult, IpcChannelPeekError)
SHMIPC_API IpcChannelPeekResult ipc_channel_peek(const ipc_channel_t *channel,
                                                 ipc_entry_t *dest);

typedef struct IpcChannelSkipError {
  uint64_t offset;
} IpcChannelSkipError;
IPC_RESULT(IpcChannelSkipResult, uint64_t, IpcChannelSkipError)
SHMIPC_API IpcChannelSkipResult ipc_channel_skip(ipc_channel_t *channel, uint64_t offset);

SHMIPC_END_DECLS
