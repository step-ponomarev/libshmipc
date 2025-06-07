#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#include "ipc_status.h"
#include <stdint.h>

typedef struct IpcBuffer IpcBuffer;
typedef struct IpcEntry {
  uint64_t size;
  void *payload;
} IpcEntry;

uint64_t ipc_allign_size(uint64_t);
IpcBuffer *ipc_buffer_create(uint8_t *, const uint64_t);
IpcBuffer *ipc_buffer_attach(uint8_t *);

IpcStatus ipc_write(IpcBuffer *, const void *, const uint64_t);
IpcStatus ipc_read(IpcBuffer *, IpcEntry *);
IpcStatus ipc_peek(IpcBuffer *, IpcEntry *);
IpcStatus ipc_delete(IpcBuffer *);

IpcStatus ipc_reserve_entry(IpcBuffer *, const uint64_t, uint8_t **);
IpcStatus ipc_commit_entry(IpcBuffer *, const uint8_t *);

#endif
