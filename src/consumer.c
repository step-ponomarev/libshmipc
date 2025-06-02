#include "ipc_buffer.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(const int argc, const char *argv[]) {
  IpcBuffer *buf = ipc_open_buffer("/22");

  IpcEntry e;
  while ((e = ipc_read(buf)).size != 0) {
    printf("%s", (char *)e.payload);
  }
}
