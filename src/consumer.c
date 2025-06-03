#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "test_shm_file_name.h"
#include <stdio.h>
#include <string.h>

int main(const int argc, const char *argv[]) {
  IpcMemorySegment segment = ipc_mmap(IPC_TEST_SHM_FILE_NAME, 1);
  IpcBuffer *buf = ipc_buffer_attach(segment.memory, segment.size);
  printf("Consumer initialized, segment: %s, size: %lld\n", segment.name,
         segment.size);

  IpcEntry entry;
  while ((entry = ipc_read(buf)).size == 0) {
    if (entry.size == 0) {
      continue;
    }
  }

  char *val = entry.payload;
  printf("%s\n", val);
}
