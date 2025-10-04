#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>

SHMIPC_BEGIN_DECLS

typedef struct IpcMemorySegment {
  char *name;
  uint64_t size;
  void *memory;
} IpcMemorySegment;

typedef struct IpcMmapError {
  const char *name;
  uint64_t requested_size;
  uint64_t aligned_size;
  long page_size;
  uint64_t existing_size;
  bool existed;
  int sys_errno;
} IpcMmapError;
IPC_RESULT(IpcMemorySegmentResult, IpcMemorySegment *, IpcMmapError)
SHMIPC_API IpcMemorySegmentResult ipc_mmap(const char *name, uint64_t size);

typedef struct IpcMmapUnmapError {
  const char *name;
  uint64_t size;
  int sys_errno;
} IpcMmapUnmapError;
IPC_RESULT_UNIT(IpcMmapUnmapResult, IpcMmapUnmapError)
SHMIPC_API IpcMmapUnmapResult ipc_unmap(IpcMemorySegment *segment);

typedef struct IpcMmapUnlinkError {
  const char *name;
  int sys_errno;
} IpcMmapUnlinkError;
IPC_RESULT_UNIT(IpcMmapUnlinkResult, IpcMmapUnlinkError)
SHMIPC_API IpcMmapUnlinkResult ipc_unlink(IpcMemorySegment *segment);

SHMIPC_END_DECLS
