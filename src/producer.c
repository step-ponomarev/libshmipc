#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "ipc_status.h"
#include "test_shm_file_name.h"
#include <_stdio.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define hello_msg "Hello my friend i am from other process!\n"

int main(const int argc, const char *argv[]) {
  IpcMemorySegment *segment = ipc_mmap(
      IPC_TEST_SHM_FILE_NAME, ipc_optimize_size(IPC_TEST_SHM_BUFFER_SIZE));

  if (segment == NULL) {
    printf("Trying reset..\n");
    ipc_reset(IPC_TEST_SHM_FILE_NAME);
    segment = ipc_mmap(IPC_TEST_SHM_FILE_NAME,
                       ipc_optimize_size(IPC_TEST_SHM_BUFFER_SIZE));

    if (segment == NULL) {
      return 1;
    }

    printf("Successful reset!\n");
  }

  IpcBuffer *buf = ipc_buffer_attach(segment->memory, segment->size);
  printf("Consumer initialized, segment: %s, size: %lld\n", segment->name,
         segment->size);

  if (argc > 1) {
    ipc_buffer_init(buf);
  }

  int msg = 1;
  int msg_neg = 1;
  for (int i = 0; i < 1000000000;) {
    const int m = ((i & 1) == 0 ? msg : msg_neg);
    IpcStatus status = ipc_write(buf, &m, sizeof(int));
    if (status != IPC_OK) {
      continue;
    }
    i++;
  }
}
