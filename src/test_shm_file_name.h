#include <stddef.h>
#include <stdint.h>
#ifndef IPC_TEST_SHM_FILE_NAME
#define IPC_TEST_SHM_FILE_NAME "test_file_name"
#define IPC_TEST_SHM_BUFFER_SIZE (1)

typedef struct MsgHeader {
  char type;
  uint32_t size;
} MsgHeader;
#endif
