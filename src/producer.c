#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "test_shm_file_name.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define hello_msg "Hello my friend i am from other process!\n"

int main(const int argc, const char *argv[]) {
  IpcMemorySegment segment = ipc_mmap(IPC_TEST_SHM_FILE_NAME, 1);
  IpcBuffer *buf = ipc_buffer_attach(segment.memory, segment.size);

  if (argc > 1) {
    ipc_buffer_init(buf);
  }

  printf("Producer initialized, segment: %s, size: %lld\n", segment.name,
         segment.size);

  char arr[] = hello_msg;
  ipc_write(buf, &arr, sizeof(hello_msg));

  const int mssg = 1;
  for (int i = 0; i < 0;) {
    if (ipc_write(buf, &arr, sizeof(hello_msg)) == 0) {
      continue;
    }
    printf("%d, ", i++);
  }
}
