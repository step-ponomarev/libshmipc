#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <stdbool.h>

SHMIPC_BEGIN_DECLS

typedef struct IpcBuffer IpcBuffer;

SHMIPC_API uint64_t ipc_buffer_get_memory_overhead(void);
SHMIPC_API uint64_t ipc_buffer_get_min_size(void);
SHMIPC_API uint64_t ipc_buffer_suggest_size(size_t desired_capacity);

typedef struct IpcBufferCreateError {
  size_t requested_size;
  size_t min_size;
  int sys_errno;
} IpcBufferCreateError;
IPC_RESULT(IpcBufferCreateResult, IpcBuffer *, IpcBufferCreateError)
SHMIPC_API IpcBufferCreateResult ipc_buffer_create(void *mem,
                                                   const size_t size);

typedef struct IpcBufferAttachError {
  size_t min_size;
} IpcBufferAttachError;
IPC_RESULT(IpcBufferAttachResult, IpcBuffer *, IpcBufferAttachError)
SHMIPC_API IpcBufferAttachResult ipc_buffer_attach(void *mem);

typedef struct IpcBufferWriteError {
  uint64_t offset;
  size_t requested_size;
  size_t required_size;
  size_t free_space;
  size_t available_contiguous;
  size_t buffer_size;
} IpcBufferWriteError;
IPC_RESULT_UNIT(IpcBufferWriteResult, IpcBufferWriteError)
SHMIPC_API IpcBufferWriteResult ipc_buffer_write(IpcBuffer *buffer,
                                                 const void *data,
                                                 const size_t size);

typedef struct IpcBufferReadError {
  uint64_t offset;
  size_t required_size;
} IpcBufferReadError;
IPC_RESULT_UNIT(IpcBufferReadResult, IpcBufferReadError)
SHMIPC_API IpcBufferReadResult ipc_buffer_read(IpcBuffer *buffer,
                                               IpcEntry *dest);

typedef struct IpcBufferPeekError {
  uint64_t offset;
} IpcBufferPeekError;
IPC_RESULT_UNIT(IpcBufferPeekResult, IpcBufferPeekError)
SHMIPC_API IpcBufferPeekResult ipc_buffer_peek(IpcBuffer *buffer,
                                               IpcEntry *dest);

typedef struct IpcBufferSkipError {
  uint64_t offset;
} IpcBufferSkipError;
IPC_RESULT(IpcBufferSkipResult, uint64_t, IpcBufferSkipError)
SHMIPC_API IpcBufferSkipResult ipc_buffer_skip(IpcBuffer *buffer,
                                               const uint64_t offset);

typedef struct IpcBufferSkipForceError {
  bool _unit;
} IpcBufferSkipForceError;
IPC_RESULT(IpcBufferSkipForceResult, uint64_t, IpcBufferSkipForceError)
SHMIPC_API IpcBufferSkipForceResult ipc_buffer_skip_force(IpcBuffer *buffer);

SHMIPC_END_DECLS
