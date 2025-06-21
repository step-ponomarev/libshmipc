#include "shmipc/ipc_common.h"
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
  assert(ipc_buffer_create(mem, ipc_buffer_allign_size(0)) != NULL);
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

  assert(ipc_buffer_write(buffer, &added_count, sizeof(size_t)) ==
         IPC_NO_SPACE_CONTIGUOUS);

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
  assert(ipc_buffer_skip_force(buffer).status == IPC_OK);

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

void test_skip() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val)) ==
         IPC_OK);

  IpcEntry entry;

  const IpcTransaction tx = ipc_buffer_peek(buffer, &entry);
  assert(tx.status == IPC_OK);
  assert(ipc_buffer_skip(buffer, tx.entry_id).status == IPC_OK);
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_EMPTY);

  free(buffer);
}

void test_double_skip() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val)) ==
         IPC_OK);

  IpcEntry entry;

  const IpcTransaction tx = ipc_buffer_peek(buffer, &entry);
  assert(tx.status == IPC_OK);
  assert(ipc_buffer_skip(buffer, tx.entry_id).status == IPC_OK);
  assert(ipc_buffer_skip(buffer, tx.entry_id).status ==
         IPC_TRANSACTION_MISS_MATCHED);
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_EMPTY);

  free(buffer);
}

void test_skip_forced() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val)) ==
         IPC_OK);

  IpcEntry entry;
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_OK);
  assert(ipc_buffer_skip_force(buffer).status == IPC_OK);
  assert(ipc_buffer_peek(buffer, &entry).status == IPC_EMPTY);

  free(buffer);
}

void test_skip_with_incorrect_id() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  assert(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val)) ==
         IPC_OK);

  IpcEntry entry;

  const IpcTransaction tx = ipc_buffer_peek(buffer, &entry);
  assert(tx.status == IPC_OK);
  assert(ipc_buffer_skip(buffer, (tx.entry_id << 1) - 1).status ==
         IPC_TRANSACTION_MISS_MATCHED);

  IpcEntry entry2;
  const IpcTransaction tx2 = ipc_buffer_peek(buffer, &entry2);
  assert(tx2.status == IPC_OK);
  assert(tx.entry_id == tx2.entry_id);

  assert(entry.size == entry2.size);

  int val1;
  memcpy(&val1, entry.payload, entry.size);

  int val2;
  memcpy(&val2, entry2.payload, entry2.size);

  assert(val1 == val2);

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

  assert(ipc_buffer_skip_force(buffer).status == IPC_OK);

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

void test_reserve_commit_read() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  int *data;

  const IpcTransaction tx =
      ipc_buffer_reserve_entry(buffer, sizeof(expected_val), ((void **)&data));
  assert(tx.status == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                    .size = sizeof(expected_val)};

  assert(ipc_buffer_read(buffer, &entry).status == IPC_NOT_READY);

  *data = expected_val;
  assert(ipc_buffer_commit_entry(buffer, tx.entry_id) == IPC_OK);
  assert(ipc_buffer_read(buffer, &entry).status == IPC_OK);
  assert(entry.size == sizeof(expected_val));

  int res;
  memcpy(&res, entry.payload, entry.size);
  assert(res == expected_val);

  free(entry.payload);
  free(buffer);
}

void test_reserve_double_commit() {
  uint8_t mem[128];
  IpcBuffer *buffer = ipc_buffer_create(mem, 128);

  const int expected_val = 12;
  int *data;

  const IpcTransaction tx =
      ipc_buffer_reserve_entry(buffer, sizeof(expected_val), ((void **)&data));
  assert(tx.status == IPC_OK);

  IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                    .size = sizeof(expected_val)};

  assert(ipc_buffer_read(buffer, &entry).status == IPC_NOT_READY);

  *data = expected_val;
  assert(ipc_buffer_commit_entry(buffer, tx.entry_id) == IPC_OK);
  assert(ipc_buffer_commit_entry(buffer, tx.entry_id) == IPC_ERR);
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
    const IpcTransaction tx =
        ipc_buffer_reserve_entry(buffer, sizeof(int), ((void **)&ptr));

    assert(tx.status == IPC_OK);
    *ptr = i;
    assert(ipc_buffer_commit_entry(buffer, tx.entry_id) == IPC_OK);
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
  run_test("skip entry", &test_skip);
  run_test("double skip entry", &test_double_skip);
  run_test("skip force entry", &test_skip_forced);
  run_test("skip with incorrect id", &test_skip_with_incorrect_id);
  run_test("fill buffer", &test_fill_buffer);
  run_test("add to full buffer", &test_add_to_full_buffer);
  run_test("wrap buffer", &test_wrap_buffer);
  run_test("reserve commit read", &test_reserve_commit_read);
  run_test("double commit", &test_reserve_double_commit);
  run_test("multiple reserve commit", &test_multiple_reserve_commit_read);

  return 0;
}
