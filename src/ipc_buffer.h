#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#include "ipc_status.h"
#include <stdint.h>

typedef struct IpcBuffer IpcBuffer;

typedef struct IpcBufferStat {
  const uint64_t buffer_size;
  const uint64_t head_pos;
  const uint64_t tail_pos;
} IpcBufferStat;

typedef struct IpcEntry {
  uint64_t size;
  void *payload;
} IpcEntry;

uint64_t ipc_optimize_size(uint64_t);

IpcBuffer *ipc_buffer_attach(uint8_t *, const uint64_t);
IpcStatus ipc_buffer_init(IpcBuffer *);

IpcStatus ipc_write(IpcBuffer *, const void *, const uint64_t);
IpcStatus ipc_read(IpcBuffer *, IpcEntry *);

IpcStatus ipc_read_lock(IpcBuffer *, IpcEntry *);
IpcStatus ipc_read_release(IpcBuffer *);

IpcStatus ipc_peek_unsafe(IpcBuffer *, IpcEntry *);

IpcBufferStat ipc_buffer_stat(const IpcBuffer *);

#endif
