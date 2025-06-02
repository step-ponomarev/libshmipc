#include "ipc_buffer.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(const int argc, const char *argv[]) {
  IpcBuffer *buf = ipc_create_buffer("test");

  while (1) {
    if (!ipc_has_message(buf)) {
      continue;
    }

    const IpcEntry batch = ipc_read(buf);
    printf("%s", (char *)batch.payload);
  }
}
