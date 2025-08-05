#include "shmipc/ipc_common.h"
#include "test_runner.h"
#include <assert.h>
#include <shmipc/ipc_mmap.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

void test_different_segment_sizes() {
  const char name[] = "test";

  IpcMmapError err;
  IpcMemorySegment *created_segment = ipc_mmap(name, 1, &err);
  assert(created_segment != NULL);

  const long page_size = sysconf(_SC_PAGESIZE);
  const IpcMemorySegment *diff_size_segment =
      ipc_mmap(name, page_size + 1, &err);

  assert(ipc_unlink(created_segment) == IPC_OK);
  assert(diff_size_segment == NULL);
  assert(err == INVALID_SIZE);
}

void test_min_segment_size() {
  const char name[] = "test";

  IpcMmapError err;
  IpcMemorySegment *created_segment = ipc_mmap(name, 1, &err);
  assert(created_segment != NULL);

  const long page_size = sysconf(_SC_PAGESIZE);
  const uint64_t size = created_segment->size;

  assert(ipc_unlink(created_segment) == IPC_OK);
  assert((uint64_t)page_size == size);
}

int main() {
  run_test("test min segment size", &test_min_segment_size);
  run_test("test different segment sizes", &test_different_segment_sizes);
  return 0;
}
