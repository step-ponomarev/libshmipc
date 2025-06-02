#include "ipc_buffer.h"
#include <stdio.h>
#include <string.h>

int main(const int argc, const char *argv[]) {
  IpcBuffer *buf = ipc_create_buffer("/22");

  int ch;
  while ((ch = getchar()) != EOF) {
    ipc_write(buf, &ch, sizeof(char));
  }
}
