#include "ipc_buffer.h"
#include "ipc_status.h"
#include "test_runner.h"
#include <_stdio.h>
#include <assert.h>
#include <stddef.h>
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

  free(entry.payload);
  free(buffer);
}

void test_fill_buffer() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  size_t added_count = 0;
  while (ipc_write(buffer, &added_count, sizeof(size_t)) == IPC_OK &&
         (++added_count))
    ;

  size_t *ptr = malloc(sizeof(size_t));
  IpcEntry entry = {.payload = ptr, .size = sizeof(size_t)};
  for (size_t i = 0; i < added_count; i++) {
    assert(ipc_read(buffer, &entry) == IPC_OK);
    assert(entry.size == sizeof(size_t));

    size_t res;
    memcpy(&res, entry.payload, entry.size);

    assert(res == i);
  }

  assert(ipc_read(buffer, &entry) == IPC_EMPTY);

  free(ptr);
  free(buffer);
}

void test_add_to_full_buffer() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  size_t added_count = 0;
  while (ipc_write(buffer, &added_count, sizeof(size_t)) == IPC_OK &&
         (++added_count))
    ;

  assert(ipc_write(buffer, &added_count, sizeof(size_t)) ==
         IPC_NO_SPACE_CONTIGUOUS);

  free(buffer);
}

void test_wrap_buffer() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  size_t added_count = 0;
  while (ipc_write(buffer, &added_count, sizeof(size_t)) == IPC_OK &&
         (++added_count))
    ;

  assert(ipc_write(buffer, &added_count, sizeof(size_t)) ==
         IPC_NO_SPACE_CONTIGUOUS);

  size_t *ptr = malloc(sizeof(size_t));
  assert(ipc_delete(buffer) == IPC_OK);

  const size_t last_val = 666;
  assert(ipc_write(buffer, &last_val, sizeof(last_val)) == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(size_t)), .size = sizeof(size_t)};

  size_t prev;

  while (ipc_read(buffer, &entry) == IPC_OK) {
    assert(entry.size == sizeof(size_t));
    memcpy(&prev, entry.payload, entry.size);
  }

  assert(prev == last_val);
  free(entry.payload);
  free(buffer);
}

void test_reserve_commit() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  int *data;
  assert(ipc_reserve_entry(buffer, sizeof(expected_val), ((uint8_t **)&data)) ==
         IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                    .size = sizeof(expected_val)};

  assert(ipc_read(buffer, &entry) == IPC_NOT_READY);

  *data = expected_val;
  assert(ipc_commit_entry(buffer, (uint8_t *)data) == IPC_OK);
  assert(ipc_read(buffer, &entry) == IPC_OK);
  assert(entry.size == sizeof(expected_val));

  int res;
  memcpy(&res, entry.payload, entry.size);
  assert(res == expected_val);

  free(entry.payload);
  free(buffer);
}

int main() {
  run_test("create too small buffer", &test_create_too_small_buffer);
  run_test("size allign funciton", &test_size_allign_funcion);
  run_test("single entry", &test_sigle_entry);
  run_test("fill buffer", &test_fill_buffer);
  run_test("add to full buffer", &test_add_to_full_buffer);
  run_test("wrap buffer", &test_wrap_buffer);
  run_test("reserve commit", &test_reserve_commit);

  return 0;
}
