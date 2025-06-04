#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "test_shm_file_name.h"
#include <_stdio.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define hello_msg "Hello my friend i am from other process!\n"

int main(const int argc, const char *argv[]) {
  IpcMemorySegment segment = ipc_mmap(IPC_TEST_SHM_FILE_NAME, 1);
  IpcBuffer *buf = ipc_buffer_attach(segment.memory, segment.size);

  if (argc > 1) {
    ipc_buffer_init(buf);
  }

  size_t size;
  char *line;
  while (((line = fgetln(stdin, &size)) != NULL) && (size > 0)) {
    ipc_write(buf, line, size);
  }
}
