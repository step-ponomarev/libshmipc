#pragma once

#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_channel.h>
#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <stddef.h>

SHMIPC_BEGIN_DECLS

typedef struct IpcInitBufferCreateError {
  size_t requested_size;
  size_t min_size;
  int sys_errno;
} IpcInitBufferCreateError;
IPC_RESULT(IpcInitBufferCreateResult, IpcBuffer *, IpcInitBufferCreateError)
SHMIPC_API IpcInitBufferCreateResult ipc_init_buffer_create(const char *name,
                                                            const size_t size);

typedef struct IpcInitBufferAttachError {
  size_t requested_size;
  size_t min_size;
  int sys_errno;
} IpcInitBufferAttachError;
IPC_RESULT(IpcInitBufferAttachResult, IpcBuffer *, IpcInitBufferAttachError)
SHMIPC_API IpcInitBufferAttachResult ipc_init_buffer_attach(const char *name,
                                                            const size_t size);

typedef struct IpcInitChannelOpenError {
  size_t requested_size;
  size_t min_size;
  int sys_errno;
  IpcChannelConfiguration config;
} IpcInitChannelOpenError;
IPC_RESULT(IpcInitChannelResult, IpcChannel *, IpcInitChannelOpenError)
SHMIPC_API IpcInitChannelResult ipc_init_channel_create(
    const char *name, const size_t size, const IpcChannelConfiguration config);

typedef struct IpcInitChannelConnectError {
  size_t min_size;
  int sys_errno;
  IpcChannelConfiguration config;
} IpcInitChannelConnectError;
IPC_RESULT(IpcInitChannelConnectResult, IpcChannel *,
           IpcInitChannelConnectError)
SHMIPC_API IpcChannelConnectResult ipc_init_channel_connect(
    const char *name, const size_t size, const IpcChannelConfiguration config);

SHMIPC_END_DECLS
