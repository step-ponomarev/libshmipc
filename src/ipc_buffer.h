#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#include "ipc_status.h"
#include <stdint.h>

typedef struct IpcBuffer IpcBuffer;

typedef struct IpcEntry {
  uint64_t size;
  void *payload;
} IpcEntry;

IpcBuffer *ipc_buffer_attach(uint8_t *, const uint64_t);
IpcStatus ipc_buffer_init(IpcBuffer *);

IpcStatus ipc_write(IpcBuffer *, const void *, const uint64_t);
IpcStatus ipc_read(IpcBuffer *, IpcEntry *);

IpcStatus ipc_read_lock(IpcBuffer *, IpcEntry *);
IpcStatus ipc_read_release(IpcBuffer *);

IpcStatus ipc_peek_unsafe(IpcBuffer *, IpcEntry *);

#endif
