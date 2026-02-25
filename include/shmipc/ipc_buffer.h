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
SHMIPC_API ipc_status_t ipc_buffer_attach(void *mem, ipc_buffer_t **out, ipc_error_t* err);
SHMIPC_API ipc_status_t ipc_buffer_write(ipc_buffer_t *buffer, const void *data, size_t size, ipc_error_t* err);
SHMIPC_API ipc_status_t ipc_buffer_read(ipc_buffer_t *buffer, ipc_entry_t *dest,  ipc_error_t* err);
SHMIPC_API ipc_status_t ipc_buffer_peek(const ipc_buffer_t *buffer, ipc_entry_t *dest, ipc_error_t* err);

typedef struct IpcBufferSkipError {
  uint64_t offset;
} IpcBufferSkipError;
IPC_RESULT(IpcBufferSkipResult, uint64_t, IpcBufferSkipError)
SHMIPC_API IpcBufferSkipResult ipc_buffer_skip(ipc_buffer_t *buffer,
                                               const uint64_t offset);

SHMIPC_END_DECLS
