#include "shmipc/ipc_common.h"
#include "test_runner.h"
#include <assert.h>
#include <shmipc/ipc_mmap.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

void test_different_segment_sizes() {
  const char name[] = "/test";

  const IpcMemorySegmentResult created_segment = ipc_mmap(name, 1);
  assert(IpcMemorySegmentResult_is_ok(created_segment));

  const long page_size = sysconf(_SC_PAGESIZE);
  const IpcMemorySegmentResult diff_size_segment =
      ipc_mmap(name, page_size + 1);
  assert(IpcMemorySegmentResult_is_error(diff_size_segment));
  assert(ipc_unlink(created_segment.result).ipc_status == IPC_OK);
  assert(diff_size_segment.ipc_status == IPC_ERR_ILLEGAL_STATE);
}

void test_min_segment_size() {
  const char name[] = "/test";

  const IpcMemorySegmentResult created_segment = ipc_mmap(name, 1);
  assert(IpcMemorySegmentResult_is_ok(created_segment));

  const long page_size = sysconf(_SC_PAGESIZE);
  const uint64_t size = created_segment.result->size;

  assert(ipc_unlink(created_segment.result).ipc_status == IPC_OK);
  assert((uint64_t)page_size == size);
}

int main() {
  run_test("test min segment size", &test_min_segment_size);
  run_test("test different segment sizes", &test_different_segment_sizes);
  return 0;
}
