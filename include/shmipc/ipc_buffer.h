#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#include "ipc_common.h"
#include <stdint.h>

typedef struct IpcBuffer IpcBuffer;

uint64_t ipc_buffer_allign_size(uint64_t);
IpcBuffer *ipc_buffer_create(void *, const uint64_t);
IpcBuffer *ipc_buffer_attach(void *);

IpcStatus ipc_buffer_write(IpcBuffer *, const void *, const uint64_t);
IpcTransaction ipc_buffer_read(IpcBuffer *, IpcEntry *);
IpcTransaction ipc_buffer_peek(IpcBuffer *, IpcEntry *);
IpcTransaction ipc_buffer_skip(IpcBuffer *, const IpcEntryId);
IpcTransaction ipc_buffer_skip_force(IpcBuffer *);

IpcTransaction ipc_buffer_reserve_entry(IpcBuffer *, const uint64_t, void **);
IpcStatus ipc_buffer_commit_entry(IpcBuffer *, const IpcEntryId);

#endif
