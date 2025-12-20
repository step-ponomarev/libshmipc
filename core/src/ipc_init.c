#include "include/shmipc/ipc_init.h"
#include "ipc_utils.h"
#include <shmipc/ipc_init.h>
#include <shmipc/ipc_mmap.h>
#include <stdint.h>
#include <unistd.h>

// TODO: rename to path
SHMIPC_API IpcInitBufferCreateResult ipc_init_buffer_create(const char *name,
                                                            const size_t size) {
  if (!IS_ALIGNED_BY_CACHE_LINE(size)) {
    // TODO: return errorb
  }

  const uint64_t buffer_overhead = ipc_buffer_get_memory_overhead();
  if (size <= buffer_overhead) {
    // TODO: error
  }

  const uint64_t data_capacity = size - buffer_overhead;
  if (!is_power_of_2(data_capacity)) {
    // TODO: errorr
  }

  const IpcMemorySegmentResult mmap = ipc_mmap(name, size);
  if (IpcMemorySegmentResult_is_error(mmap)) {
    // TODO: return error
  }

  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mmap.result, size);
  if (IpcBufferCreateResult_is_error(buffer_result)) {
    // TODO: return error
  }

  return IpcInitBufferCreateResult_ok(IPC_OK, buffer_result.result);
}

SHMIPC_API IpcInitBufferAttachResult ipc_init_buffer_attach(const char *name,
                                                            const size_t size) {
  if (!IS_ALIGNED_BY_CACHE_LINE(size)) {
    // TODO: return errorb
  }

  const uint64_t buffer_overhead = ipc_buffer_get_memory_overhead();
  if (size <= buffer_overhead) {
    // TODO: error
  }

  const uint64_t data_capacity = size - buffer_overhead;
  if (!is_power_of_2(data_capacity)) {
    // TODO: errorr
  }

  const IpcMemorySegmentResult mmap = ipc_mmap(name, size);
  if (IpcMemorySegmentResult_is_error(mmap)) {
    // TODO: return error
  }

  const IpcBufferAttachResult buffer_result = ipc_buffer_attach(mmap.result);
  if (IpcBufferAttachResult_is_error(buffer_result)) {
    // TODO: return error
  }

  return IpcInitBufferAttachResult_ok(IPC_OK, buffer_result.result);
}

SHMIPC_API IpcInitChannelResult ipc_init_channel_create(
    const char *name, const size_t size, const IpcChannelConfiguration config) {

}

SHMIPC_API IpcChannelConnectResult ipc_init_channel_connect(
    const char *name, const size_t size, const IpcChannelConfiguration config) {

}
