#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <shmipc/ipc_error.h>
#include <shmipc/ipc_status.h>
#include <stdbool.h>

SHMIPC_BEGIN_DECLS

typedef struct ipc_buffer_t ipc_buffer_t;

SHMIPC_API uint64_t ipc_buffer_get_memory_overhead(void);
SHMIPC_API uint64_t ipc_buffer_get_min_size(void);
SHMIPC_API uint64_t ipc_buffer_suggest_size(size_t desired_capacity);

SHMIPC_API ipc_status_t ipc_buffer_create(void *mem, size_t size, ipc_buffer_t **out, ipc_error_t* err);

typedef struct IpcBufferAttachError {
  size_t min_size;
  int sys_errno;
} IpcBufferAttachError;
IPC_RESULT(IpcBufferAttachResult, ipc_buffer_t *, IpcBufferAttachError)
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
SHMIPC_API IpcBufferWriteResult ipc_buffer_write(ipc_buffer_t *buffer,
                                                 const void *data,
                                                 const size_t size);

typedef struct IpcBufferReadError {
  uint64_t offset;
  size_t required_size;
} IpcBufferReadError;
IPC_RESULT_UNIT(IpcBufferReadResult, IpcBufferReadError)
SHMIPC_API IpcBufferReadResult ipc_buffer_read(ipc_buffer_t *buffer,
                                               IpcEntry *dest);

typedef struct IpcBufferPeekError {
  uint64_t offset;
} IpcBufferPeekError;
IPC_RESULT_UNIT(IpcBufferPeekResult, IpcBufferPeekError)
SHMIPC_API IpcBufferPeekResult ipc_buffer_peek(ipc_buffer_t *buffer,
                                               IpcEntry *dest);

typedef struct IpcBufferSkipError {
  uint64_t offset;
} IpcBufferSkipError;
IPC_RESULT(IpcBufferSkipResult, uint64_t, IpcBufferSkipError)
SHMIPC_API IpcBufferSkipResult ipc_buffer_skip(ipc_buffer_t *buffer,
                                               const uint64_t offset);

typedef struct IpcBufferSkipForceError {
  bool _unit;
} IpcBufferSkipForceError;
IPC_RESULT(IpcBufferSkipForceResult, uint64_t, IpcBufferSkipForceError)
SHMIPC_API IpcBufferSkipForceResult ipc_buffer_skip_force(ipc_buffer_t *buffer);

SHMIPC_END_DECLS
