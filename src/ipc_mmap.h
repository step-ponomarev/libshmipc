#ifndef IPC_MMAP_H
#define IPC_MMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shmipc/ipc_common.h>
#include <stdint.h>

// TODO: incapsulate
typedef struct IpcMemorySegment {
  char *name;
  uint64_t size;
  void *memory;
} IpcMemorySegment;

IpcMemorySegment *ipc_mmap(const char *, const uint64_t);
IpcStatus ipc_unmap(IpcMemorySegment *);
IpcStatus ipc_unlink(const IpcMemorySegment *);
IpcStatus ipc_reset(const char *);

#ifdef __cplusplus
}
#endif

#endif
