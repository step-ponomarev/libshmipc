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

IpcBuffer *ipc_buffer_attach(uint8_t *, const uint64_t);
IpcStatus ipc_buffer_init(IpcBuffer *);

IpcStatus ipc_write(IpcBuffer *, const void *, const uint64_t);
IpcStatus ipc_read(IpcBuffer *, IpcEntry *);
IpcStatus ipc_peek(IpcBuffer *, IpcEntry *);
IpcStatus ipc_delete(IpcBuffer *);

#endif
