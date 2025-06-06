#include "ipc_buffer.h"
#include "ipc_mmap.h"
#include "ipc_status.h"
#include "test_shm_file_name.h"
#include <_stdio.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *hello_msg = "Hello my friend i am from other process!\n";
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

  IpcBuffer *buf = ipc_buffer_attach(segment->memory, segment->size);
  printf("Consumer initialized, segment: %s, size: %lld\n", segment->name,
         segment->size);

  if (argc > 1) {
    ipc_buffer_init(buf);
  }
  const char mode = argc == 1 ? DIGITS_MODE : STRING_MODE;

  const size_t str_len = strlen(hello_msg);
  const int size =
      sizeof(char) + ((mode == DIGITS_MODE) ? sizeof(int) : str_len);

  int msg = 1;
  int msg_neg = 1;
  for (int i = 0; i < 1000000000;) {
    char msg[size];
    memcpy(msg, &mode, sizeof(char));
    memcpy(msg + sizeof(char), (mode == DIGITS_MODE) ? &i : &hello_msg,
           (mode == DIGITS_MODE) ? sizeof(int) : str_len);
    ipc_write(buf, &msg, size) == IPC_OK &&i++;
  }
}
