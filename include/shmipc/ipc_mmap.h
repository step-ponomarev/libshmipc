#ifndef _IPC_MMAP_H
#define _IPC_MMAP_H

#include "ipc_status.h"
#include <stdint.h>

// TODO: incapsulate
typedef struct IpcMemorySegment {
  char *name;
  uint64_t size;
  uint8_t *memory;
} IpcMemorySegment;

IpcMemorySegment *ipc_mmap(const char *, const uint64_t);
IpcStatus ipc_unmap(IpcMemorySegment *);
IpcStatus ipc_unlink(const IpcMemorySegment *);
IpcStatus ipc_reset(const char *);

#endif
