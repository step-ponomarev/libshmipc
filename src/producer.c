#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "test_shm_file_name.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(const int argc, const char *argv[]) {
  IpcMemorySegment segment = ipc_mmap(IPC_TEST_SHM_FILE_NAME, 1);
  IpcBuffer *buf = ipc_buffer_attach(segment.memory, segment.size);
  ipc_buffer_init(buf);

  printf("Producer initialized, segment: %s, size: %lld\n", segment.name,
         segment.size);

  for (int i = 0; i < INT_MAX;) {
    if (!ipc_write(buf, &i, sizeof(int))) {
      printf("DEBUG: ipc write is failed on i=%d\n", i);
      continue;
    }
    i++;
  }
}
