#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "ipc_status.h"
#include "test_shm_file_name.h"
#include <stdio.h>
#include <string.h>

int main(const int argc, const char *argv[]) {
  IpcMemorySegment *segment = ipc_mmap(
      IPC_TEST_SHM_FILE_NAME, ipc_optimize_size(IPC_TEST_SHM_BUFFER_SIZE));

  IpcBuffer *buf = ipc_buffer_attach(segment->memory, segment->size);
  printf("Consumer initialized, segment: %s, size: %lld\n", segment->name,
         segment->size);

  int res = 0;
  IpcEntry entry;
  while (1) {
    IpcStatus status = ipc_read(buf, &entry);
    if (status != IPC_OK) {
      continue;
    }

    int *val = entry.payload;
    printf("%d\n", res += *val);
  }
}
