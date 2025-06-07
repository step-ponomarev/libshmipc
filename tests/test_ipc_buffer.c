#include "ipc_buffer.h"
#include "ipc_status.h"
#include "test_runner.h"
#include <_stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_create_too_small_buffer() {
  uint8_t mem[128];
  assert(ipc_buffer_create(mem, 0) == NULL);
}

void test_size_allign_funcion() {
  uint8_t mem[128];

  IpcBuffer *buffer = ipc_buffer_create(mem, ipc_allign_size(0));
  assert(buffer == NULL);

  buffer = ipc_buffer_create(mem, ipc_allign_size(1));
  assert(buffer == NULL);

  buffer = ipc_buffer_create(mem, ipc_allign_size(2));
  assert(buffer != NULL);

  free(buffer);
}

void test_sigle_entry() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_write(buffer, &expected_val, sizeof(expected_val)) == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                    .size = sizeof(expected_val)};

  assert(ipc_read(buffer, &entry) == IPC_OK);
  assert(entry.size == sizeof(expected_val));

  int res;
  memcpy(&res, entry.payload, entry.size);

  assert(res == expected_val);

  free(buffer);
}

int main() {
  run_test("create too small buffer", &test_create_too_small_buffer);
  run_test("test size allign funciton", &test_size_allign_funcion);
  run_test("test single entry", &test_sigle_entry);

  printf("test passed\n");
  return 0;
}
