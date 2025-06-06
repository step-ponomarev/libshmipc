#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "ipc_status.h"
#include "test_shm_file_name.h"
#include <_stdio.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char hello_msg[] = "Hello my friend i am from other process!\n";
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
  const MsgHeader header = {.type = mode,
                            .size = mode == DIGITS_MODE ? sizeof(int)
                                                        : strlen(hello_msg)};
  for (int i = 0; i < 100000000;) {
    if (i % 8 == 0) {
      mode = STRING_MODE;
    } else {
      mode = DIGITS_MODE;
    }

    const MsgHeader header = {.type = mode,
                              .size = mode == DIGITS_MODE ? sizeof(int)
                                                          : strlen(hello_msg)};
    char msg[sizeof(header) + header.size];
    memcpy(msg, &header, sizeof(header));
    memcpy(msg + sizeof(header), (mode == DIGITS_MODE) ? &i : hello_msg,
           header.size);

    if (ipc_write(buf, msg, sizeof(header) + header.size) != IPC_OK) {
      continue;
    };
    i++;
  }
}
