#include "test_runner.h"
#include <assert.h>
#include <shmipc/ipc_buffer.h>
#include <stddef.h>
#include <stdint.h>
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

  const int eval = 12;
  assert(ipc_buffer_write(buffer, &eval, sizeof(eval)) == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(eval)), .size = sizeof(eval)};

  assert(ipc_buffer_read(buffer, &entry).status == IPC_OK);
  assert(entry.size == sizeof(eval));

  int res;
  memcpy(&res, entry.payload, entry.size);

  assert(res == eval);

  free(entry.payload);
  free(buffer);
}

void test_fill_buffer() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  size_t added_count = 0;
  while (ipc_buffer_write(buffer, &added_count, sizeof(size_t)) == IPC_OK &&
         (++added_count))
    ;

  size_t *ptr = malloc(sizeof(size_t));
  IpcEntry entry = {.payload = ptr, .size = sizeof(size_t)};
  for (size_t i = 0; i < added_count; i++) {
    assert(ipc_buffer_read(buffer, &entry).status == IPC_OK);
    assert(entry.size == sizeof(size_t));

    size_t res;
    memcpy(&res, entry.payload, entry.size);

    assert(res == i);
  }

  assert(ipc_buffer_read(buffer, &entry).status == IPC_EMPTY);

  free(ptr);
  free(buffer);
}

void test_add_to_full_buffer() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  size_t added_count = 0;
  while (ipc_buffer_write(buffer, &added_count, sizeof(size_t)) == IPC_OK &&
         (++added_count))
    ;

  assert(ipc_buffer_write(buffer, &added_count, sizeof(size_t)) ==
         IPC_NO_SPACE_CONTIGUOUS);

  free(buffer);
}

void test_wrap_buffer() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  size_t added_count = 0;
  while (ipc_buffer_write(buffer, &added_count, sizeof(size_t)) == IPC_OK &&
         (++added_count))
    ;

  assert(ipc_buffer_write(buffer, &added_count, sizeof(size_t)) ==
         IPC_NO_SPACE_CONTIGUOUS);

  size_t *ptr = malloc(sizeof(size_t));
  assert(ipc_buffer_skip_force(buffer) == IPC_OK);

  const size_t last_val = 666;
  assert(ipc_buffer_write(buffer, &last_val, sizeof(last_val)) == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(size_t)), .size = sizeof(size_t)};

  size_t prev;

  while (ipc_buffer_read(buffer, &entry).status == IPC_OK) {
    assert(entry.size == sizeof(size_t));
    memcpy(&prev, entry.payload, entry.size);
  }

  assert(prev == last_val);
  free(entry.payload);
  free(buffer);
}

void test_peek() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val)) ==
         IPC_OK);

  IpcEntry entry;
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_OK);
  assert(entry.size == sizeof(expected_val));

  int val;
  memcpy(&val, entry.payload, sizeof(expected_val));
  assert(expected_val == val);

  entry.payload = malloc(sizeof(expected_val));
  entry.size = sizeof(expected_val);

  assert(ipc_buffer_read(buffer, &entry).status == IPC_OK);
  memcpy(&val, entry.payload, sizeof(expected_val));
  assert(expected_val == val);

  free(entry.payload);

  assert(ipc_buffer_peek(buffer, &entry).status == IPC_EMPTY);

  free(buffer);
}

void test_delete() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val)) ==
         IPC_OK);

  IpcEntry entry;
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_OK);
  assert(ipc_buffer_skip_force(buffer) == IPC_OK);
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_EMPTY);

  free(buffer);
}

void test_peek_consistency() {
  uint8_t mem[256];
  IpcBuffer *buffer = ipc_buffer_create(mem, 256);

  int v1 = 1, v2 = 2;
  assert(ipc_buffer_write(buffer, &v1, sizeof(v1)) == IPC_OK);
  assert(ipc_buffer_write(buffer, &v2, sizeof(v2)) == IPC_OK);

  IpcEntry entry;
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_OK);

  int seen;
  memcpy(&seen, entry.payload, sizeof(seen));
  assert(seen == v1);

  assert(ipc_buffer_skip_force(buffer) == IPC_OK);

  assert(ipc_buffer_peek(buffer, &entry).status == IPC_OK);
  memcpy(&seen, entry.payload, sizeof(seen));
  assert(seen == v2);

  free(buffer);
}

void test_read_too_small() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  int val = 42;
  assert(ipc_buffer_write(buffer, &val, sizeof(val)) == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(val) - 1),
                    .size = sizeof(val) - 1};

  assert(ipc_buffer_read(buffer, &entry).status == IPC_ERR_TOO_SMALL);

  free(entry.payload);
  free(buffer);
}

void test_reserve_commit_read_read() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  int *data;
  assert(ipc_buffer_reserve_entry(buffer, sizeof(expected_val),
                                  ((void **)&data)) == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                    .size = sizeof(expected_val)};

  assert(ipc_buffer_read(buffer, &entry).status == IPC_NOT_READY);

  *data = expected_val;
  assert(ipc_buffer_commit_entry(buffer, (uint8_t *)data) == IPC_OK);
  assert(ipc_buffer_read(buffer, &entry).status == IPC_OK);
  assert(entry.size == sizeof(expected_val));

  int res;
  memcpy(&res, entry.payload, entry.size);
  assert(res == expected_val);

  free(entry.payload);
  free(buffer);
}

void test_multiple_reserve_commit_read() {
  uint8_t mem[1024];
  IpcBuffer *buffer = ipc_buffer_create(mem, 1024);

  for (int i = 0; i < 10; ++i) {
    int *ptr;
    assert(ipc_buffer_reserve_entry(buffer, sizeof(int), (void **)&ptr) ==
           IPC_OK);
    *ptr = i;
    assert(ipc_buffer_commit_entry(buffer, (uint8_t *)ptr) == IPC_OK);
  }

  int *buf = malloc(sizeof(int));
  IpcEntry entry = {.payload = buf, .size = sizeof(int)};
  for (int i = 0; i < 10; ++i) {
    assert(ipc_buffer_read(buffer, &entry).status == IPC_OK);
    assert(*buf == i);
  }

  free(buf);
  free(buffer);
}

int main() {
  run_test("create too small buffer", &test_create_too_small_buffer);
  run_test("size allign funciton", &test_size_allign_funcion);
  run_test("single entry", &test_sigle_entry);
  run_test("peek entry", &test_peek);
  run_test("peek consistency", &test_peek_consistency);
  run_test("read too small", &test_read_too_small);
  run_test("delete entry", &test_delete);
  run_test("fill buffer", &test_fill_buffer);
  run_test("add to full buffer", &test_add_to_full_buffer);
  run_test("wrap buffer", &test_wrap_buffer);
  run_test("reserve commit read", &test_reserve_commit_read_read);
  run_test("multiple reserve commit", &test_multiple_reserve_commit_read);

  return 0;
}
