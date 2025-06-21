#include "../src/ipc_utils.h"
#include "shmipc/ipc_buffer.h"
#include "test_runner.h"
#include <_time.h>
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

  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  const int expected = 42;
  assert(ipc_channel_write(channel, &expected, sizeof(expected)) == IPC_OK);

  IpcEntry entry;
  assert(ipc_channel_read(channel, &entry).status == IPC_OK);

  int res;
  memcpy(&res, entry.payload, entry.size);

  assert(expected == res);

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

int main() {
  run_test("write read", &test_write_read);
  run_test("invalid config", &test_invalid_config);
  run_test("write try read", &test_write_try_read);
  run_test("try read empty", &test_try_read_empty);
  run_test("write too large entry", &test_write_too_large_entry);
  run_test("read retry limit exceeded", &test_read_retry_limit_reacehed);
  run_test("skip corrupted entry", &test_skip_corrupted_entry);
  run_test("read timeout", &test_read_timeout);

  return 0;
}
