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

  IpcBuffer *buf = ipc_buffer_attach(segment->memory);
  printf("Consumer initialized, segment: %s, size: %lld\n", segment->name,
         segment->size);

  if (buf == NULL) {
    return 1;
  }

  IpcEntry entry;
  while (1) {
    if (ipc_read(buf, &entry) != IPC_OK) {
      continue;
    }

    MsgHeader *header = (MsgHeader *)entry.payload;
    if (header->type == DIGITS_MODE) {
      int d;
      memcpy(&d, ((char *)header) + sizeof(MsgHeader), header->size);
      printf("Recive digit: %d\n", d);
    } else {
      printf("Recive strung: %s\n", ((char *)(header) + sizeof(MsgHeader)));
    }
  }

  return 0;
}
