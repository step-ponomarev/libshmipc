#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "ipc_status.h"
#include "test_shm_file_name.h"
#include <_stdio.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

const char hello_msg[] = "Hello my friend i am from other process!\n";
#define DIGITS_MODE 1
#define STRING_MODE 2

int main(const int argc, const char *argv[]) {
  IpcMemorySegment *segment = ipc_mmap(
      IPC_TEST_SHM_FILE_NAME, ipc_allign_size(IPC_TEST_SHM_BUFFER_SIZE));

  if (segment == NULL) {
    printf("Trying reset..\n");
    ipc_reset(IPC_TEST_SHM_FILE_NAME);
    segment = ipc_mmap(IPC_TEST_SHM_FILE_NAME,
                       ipc_allign_size(IPC_TEST_SHM_BUFFER_SIZE));

    if (segment == NULL) {
      return 1;
    }

    printf("Successful reset!\n");
  }

  IpcBuffer *buf = ipc_buffer_create(segment->memory, segment->size);
  printf("Prodiucer initialized, segment: %s, size: %lld\n", segment->name,
         segment->size);

  if (buf == NULL) {
    return 1;
  }

  char mode = argc == 1 ? DIGITS_MODE : STRING_MODE;
  for (int i = 0; i < 1024;) {
    if (ipc_write(buf, hello_msg, (strlen(hello_msg) + 1)) != IPC_OK) {
      continue;
    };
    i++;
  }
}
