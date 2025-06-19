#include "shmipc/ipc_buffer.h"
#include "test_runner.h"
#include <assert.h>
#include <shmipc/ipc_channel.h>
#include <shmipc/ipc_common.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void test_read_retry_limit_exceeded() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];

  const IpcChannelConfiguration config = {
      .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

  IpcChannel *channel = ipc_channel_create(mem, size, config);
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

  const IpcChannelConfiguration config = {
      .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

  IpcChannel *channel = ipc_channel_create(mem, size, config);
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

  const IpcChannelConfiguration config = {
      .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

  IpcChannel *channel = ipc_channel_create(mem, size, config);
  const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
  IpcEntry entry;
  assert(ipc_channel_read_with_timeout(channel, &entry, &timeout).status ==
         IPC_TIMEOUT);

  ipc_channel_destroy(channel);
}

int main() {
  run_test("read retry limit exceeded", &test_read_retry_limit_exceeded);
  run_test("skip corrupted entry", &test_skip_corrupted_entry);
  run_test("read timeout", &test_read_timeout);

  return 0;
}
