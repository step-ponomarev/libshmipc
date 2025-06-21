#include "../src/ipc_utils.h"
#include "shmipc/ipc_buffer.h"
#include "test_runner.h"
#include <assert.h>
#include <shmipc/ipc_channel.h>
#include <shmipc/ipc_common.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const IpcChannelConfiguration DEFAULT_CONFIG = {
    .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

void test_invalid_config() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  assert(ipc_channel_create(mem, size,
                            (IpcChannelConfiguration){.max_round_trips = 0,
                                                      .start_sleep_ns = 10,
                                                      .max_sleep_ns = 150}) ==
         NULL);

  assert(ipc_channel_create(mem, size,
                            (IpcChannelConfiguration){.max_round_trips = 1,
                                                      .start_sleep_ns = 0,
                                                      .max_sleep_ns = 150}) ==
         NULL);

  assert(ipc_channel_create(mem, size,
                            (IpcChannelConfiguration){.max_round_trips = 1,
                                                      .start_sleep_ns = 1,
                                                      .max_sleep_ns = 0}) ==
         NULL);

  assert(ipc_channel_create(mem, size,
                            (IpcChannelConfiguration){.max_round_trips = 1,
                                                      .start_sleep_ns = 1,
                                                      .max_sleep_ns = 0}) ==
         NULL);

  assert(ipc_channel_create(mem, size,
                            (IpcChannelConfiguration){.max_round_trips = 1,
                                                      .start_sleep_ns = 100,
                                                      .max_sleep_ns = 90}) ==
         NULL);
}

void test_write_too_large_entry() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  const size_t entry_size = sizeof(uint8_t) * 1024;
  void *payload = malloc(entry_size);
  assert(ipc_channel_write(channel, payload, entry_size) ==
         IPC_ERR_ENTRY_TOO_LARGE);

  free(payload);
  ipc_channel_destroy(channel);
}

void test_write_read() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *producer = ipc_channel_create(mem, size, DEFAULT_CONFIG);
  assert(producer != NULL);

  const int val = 43;
  assert(ipc_channel_write(producer, &val, sizeof(val)) == IPC_OK);

  IpcChannel *consumer = ipc_channel_connect(mem, DEFAULT_CONFIG);
  assert(consumer != NULL);

  IpcEntry entry;
  assert(ipc_channel_read(consumer, &entry).status == IPC_OK);

  int res;
  memcpy(&res, entry.payload, sizeof(res));
  assert(res == val);

  ipc_channel_destroy(producer);
  ipc_channel_destroy(consumer);
}

void test_destroy_null() {
  assert(ipc_channel_destroy(NULL) == IPC_ERR_INVALID_ARGUMENT);
}

void test_peek() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  const int expected = 42;
  assert(ipc_channel_write(channel, &expected, sizeof(expected)) == IPC_OK);

  IpcEntry entry;
  IpcTransaction tx = ipc_channel_peek(channel, &entry);
  assert(tx.status == IPC_OK);
  assert(entry.size == sizeof(expected));

  int peeked;
  memcpy(&peeked, entry.payload, sizeof(expected));
  assert(peeked == expected);

  IpcEntry entry2;
  IpcTransaction tx2 = ipc_channel_read(channel, &entry2);
  assert(tx2.status == IPC_OK);
  int read_val;
  memcpy(&read_val, entry2.payload, sizeof(read_val));
  assert(read_val == expected);

  ipc_channel_destroy(channel);
}

void test_peek_empty() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  IpcEntry entry;
  IpcTransaction tx = ipc_channel_peek(channel, &entry);
  assert(tx.status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}

void test_write_try_read() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  const int expected = 42;
  assert(ipc_channel_write(channel, &expected, sizeof(expected)) == IPC_OK);

  IpcEntry entry;
  assert(ipc_channel_try_read(channel, &entry).status == IPC_OK);

  int res;
  memcpy(&res, entry.payload, entry.size);

  assert(expected == res);

  ipc_channel_destroy(channel);
}

void test_try_read_empty() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  IpcEntry entry;
  assert(ipc_channel_try_read(channel, &entry).status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}

void test_read_retry_limit_reacehed() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);
  IpcBuffer *buf = ipc_buffer_attach(mem);

  const int expected = -11;

  void *dest;
  IpcTransaction tx = ipc_buffer_reserve_entry(buf, sizeof(expected), &dest);
  memcpy(dest, &expected, sizeof(expected));

  IpcEntry entry;
  assert(ipc_channel_read(channel, &entry).status == IPC_REACHED_RETRY_LIMIT);

  ipc_buffer_commit_entry(buf, tx.entry_id);
  assert(ipc_channel_read(channel, &entry).status == IPC_OK);

  int res;
  memcpy(&res, entry.payload, sizeof(expected));
  assert(expected == res);

  ipc_channel_destroy(channel);
  free(buf);
}

void test_skip_corrupted_entry() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);
  IpcBuffer *buf = ipc_buffer_attach(mem);

  void *dest;

  const int expected = -11;
  IpcTransaction not_commited_tx =
      ipc_buffer_reserve_entry(buf, sizeof(expected), &dest);

  IpcTransaction commited_tx =
      ipc_buffer_reserve_entry(buf, sizeof(expected), &dest);
  memcpy(dest, &expected, sizeof(expected));

  ipc_buffer_commit_entry(buf, commited_tx.entry_id);

  IpcEntry entry;
  IpcTransaction read_tx = ipc_channel_read(channel, &entry);
  assert(read_tx.status == IPC_REACHED_RETRY_LIMIT);
  assert(read_tx.entry_id == not_commited_tx.entry_id);
  assert(ipc_channel_skip(channel, read_tx.entry_id).status == IPC_OK);

  read_tx = ipc_channel_read(channel, &entry);
  assert(read_tx.status == IPC_OK);
  assert(read_tx.entry_id == commited_tx.entry_id);

  int res;
  memcpy(&res, entry.payload, sizeof(expected));
  assert(expected == res);

  ipc_channel_destroy(channel);
  free(buf);
}

void test_skip_force() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  const int val = 42;
  assert(ipc_channel_write(channel, &val, sizeof(val)) == IPC_OK);

  IpcEntry entry;
  assert(ipc_channel_peek(channel, &entry).status == IPC_OK);
  assert(ipc_channel_skip_force(channel).status == IPC_OK);
  assert(ipc_channel_peek(channel, &entry).status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}

void test_read_timeout() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);
  const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
  const uint64_t timeout_ns = ipc_timespec_to_nanos(&timeout);

  struct timespec time;
  assert(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
  const uint64_t before_ns = ipc_timespec_to_nanos(&time);

  IpcEntry entry;
  assert(ipc_channel_read_with_timeout(channel, &entry, &timeout).status ==
         IPC_TIMEOUT);

  assert(clock_gettime(CLOCK_MONOTONIC, &time) == 0);

  const uint64_t after_ns = ipc_timespec_to_nanos(&time);
  assert(after_ns - before_ns >= timeout_ns);

  ipc_channel_destroy(channel);
}

void test_channel_read_before_commit_via_channel() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);
  IpcBuffer *buf = ipc_buffer_attach(mem);

  const int expected = 42;

  void *dest;
  IpcTransaction tx_res = ipc_buffer_reserve_entry(buf, sizeof(int), &dest);
  assert(tx_res.status == IPC_OK);
  *((int *)dest) = expected;

  IpcEntry entry;
  IpcTransaction tx_read = ipc_channel_read(channel, &entry);
  assert(tx_read.status == IPC_REACHED_RETRY_LIMIT);

  assert(ipc_buffer_commit_entry(buf, tx_res.entry_id) == IPC_OK);

  tx_read = ipc_channel_read(channel, &entry);
  assert(tx_read.status == IPC_OK);

  int v;
  memcpy(&v, entry.payload, sizeof(v));
  assert(v == expected);

  free(entry.payload);
  ipc_channel_destroy(channel);
  free(buf);
}

void test_channel_double_commit() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);
  IpcBuffer *buf = ipc_buffer_attach(mem);

  void *dest;
  IpcTransaction tx = ipc_buffer_reserve_entry(buf, sizeof(int), &dest);
  assert(tx.status == IPC_OK);
  *(int *)dest = 42;
  assert(ipc_buffer_commit_entry(buf, tx.entry_id) == IPC_OK);
  assert(ipc_buffer_commit_entry(buf, tx.entry_id) == IPC_ERR);

  IpcEntry entry;
  IpcTransaction read1 = ipc_channel_read(channel, &entry);
  assert(read1.status == IPC_OK);
  IpcTransaction read2 = ipc_channel_try_read(channel, &entry);
  assert(read2.status == IPC_EMPTY);

  free(entry.payload);
  ipc_channel_destroy(channel);
  free(buf);
}

int main() {
  run_test("write read", &test_write_read);
  run_test("write peek", &test_peek);
  run_test("peek empty", &test_peek_empty);
  run_test("invalid config", &test_invalid_config);
  run_test("write try read", &test_write_try_read);
  run_test("destroy null", &test_destroy_null);
  run_test("try read empty", &test_try_read_empty);
  run_test("write too large entry", &test_write_too_large_entry);
  run_test("read retry limit exceeded", &test_read_retry_limit_reacehed);
  run_test("skip corrupted entry", &test_skip_corrupted_entry);
  run_test("skip force", &test_skip_force);
  run_test("read timeout", &test_read_timeout);
  run_test("read before commit", &test_channel_read_before_commit_via_channel);
  run_test("channel read after double commmit", &test_channel_double_commit);

  return 0;
}
