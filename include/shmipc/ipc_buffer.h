#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <stdbool.h>

SHMIPC_BEGIN_DECLS

typedef struct IpcBuffer IpcBuffer;

SHMIPC_API uint64_t ipc_buffer_align_size(size_t size);

typedef struct IpcBufferCreateError {
  size_t requested_size;
  size_t min_size;
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
  IpcEntryId entry_id;
  size_t requested_size;
  size_t available_contiguous;
  size_t buffer_size;
} IpcBufferWriteError;
IPC_RESULT_UNIT(IpcBufferWriteResult, IpcBufferWriteError)
SHMIPC_API IpcBufferWriteResult ipc_buffer_write(IpcBuffer *buffer,
                                                 const void *data,
                                                 const size_t size);

typedef struct IpcBufferReadError {
  IpcEntryId entry_id;
  size_t required_size;
} IpcBufferReadError;
IPC_RESULT_UNIT(IpcBufferReadResult, IpcBufferReadError)
SHMIPC_API IpcBufferReadResult ipc_buffer_read(IpcBuffer *buffer,
                                               IpcEntry *dest);

typedef struct IpcBufferPeekError {
  IpcEntryId entry_id;
} IpcBufferPeekError;
IPC_RESULT_UNIT(IpcBufferPeekResult, IpcBufferPeekError)
SHMIPC_API IpcBufferPeekResult ipc_buffer_peek(const IpcBuffer *buffer,
                                               IpcEntry *dest);

typedef struct IpcBufferSkipError {
  IpcEntryId entry_id;
} IpcBufferSkipError;
IPC_RESULT(IpcBufferSkipResult, IpcEntryId, IpcBufferSkipError)
SHMIPC_API IpcBufferSkipResult ipc_buffer_skip(IpcBuffer *buffer,
                                               const IpcEntryId id);

typedef struct IpcBufferSkipForceError {
  bool _unit;
} IpcBufferSkipForceError;
IPC_RESULT(IpcBufferSkipForceResult, IpcEntryId, IpcBufferSkipForceError)
SHMIPC_API IpcBufferSkipForceResult ipc_buffer_skip_force(IpcBuffer *buffer);

typedef struct IpcBufferReserveEntryError {
  IpcEntryId entry_id;
  uint64_t buffer_size;
  size_t required_size;
  size_t free_space;
} IpcBufferReserveEntryError;
IPC_RESULT(IpcBufferReserveEntryResult, IpcEntryId, IpcBufferReserveEntryError)
SHMIPC_API IpcBufferReserveEntryResult
ipc_buffer_reserve_entry(IpcBuffer *buffer, const size_t size, void **dest);

typedef struct IpcBufferCommitEntryError {
  IpcEntryId entry_id;
} IpcBufferCommitEntryError;
IPC_RESULT_UNIT(IpcBufferCommitEntryResult, IpcBufferCommitEntryError)
SHMIPC_API IpcBufferCommitEntryResult
ipc_buffer_commit_entry(IpcBuffer *buffer, const IpcEntryId id);

SHMIPC_END_DECLS
