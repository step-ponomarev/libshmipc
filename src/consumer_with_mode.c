#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "ipc_status.h"
#include "test_shm_file_name.h"
#include <stdio.h>
#include <string.h>

#define DIGITS_MODE 1
#define STRING_MODE 2

int main(const int argc, const char *argv[]) {
  IpcMemorySegment *segment = ipc_mmap(
      IPC_TEST_SHM_FILE_NAME, ipc_allign_size(IPC_TEST_SHM_BUFFER_SIZE));

  IpcBuffer *buf = ipc_buffer_attach(segment->memory, segment->size);
  printf("Consumer initialized, segment: %s, size: %lld\n", segment->name,
         segment->size);

  const char mode = argc == 1 ? DIGITS_MODE : STRING_MODE;

  IpcEntry entry;
  while (1) {
    IpcStatus status = ipc_lock_read(buf);
    if (status != IPC_OK) {
      fprintf(stderr, "Lock is failed!\n");
    }
    printf("Lock\n");

    if (ipc_peek_unsafe(buf, &entry) == IPC_OK) {
      void *payload = entry.payload;
      char *got_mode = (char *)(payload);
      if (*got_mode == mode) {
        got_mode++;
        if (mode == DIGITS_MODE) {
          int d;
          memcpy(&d, (void *)got_mode, sizeof(int));
          printf("Got: %d\n", d);
        } else {
          printf("Got: %s\n", got_mode);
        }
      }
    }

    if (ipc_release_read(buf) != IPC_OK) {
      fprintf(stderr, "Release is failed!\n");
    }
    printf("Unlock\n");
  }

  return 0;
}
