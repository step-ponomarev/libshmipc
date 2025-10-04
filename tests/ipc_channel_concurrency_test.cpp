#include "concurrent_set.hpp"
#include "shmipc/ipc_channel.h"
#include "test_runner.h"
#include <assert.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <string.h>
#include <thread>
#include <vector>

static const IpcChannelConfiguration DEFAULT_CONFIG = {
    .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

void produce(IpcChannel *channel, const size_t from, const size_t to) {
  for (size_t i = from; i < to;) {
    IpcChannelWriteResult status =
        ipc_channel_write(channel, &i, sizeof(size_t));
    if (status.ipc_status != IPC_OK) {
      continue;
    }
    i++;
  }
}

void consume(IpcChannel *channel, const size_t expected,
             std::shared_ptr<concurrent_set<size_t>> dest) {
  IpcEntry e;
  while (true) {
    if (dest->size() == expected)
      break;

    IpcChannelReadResult rx = ipc_channel_read(channel, &e);
    if (rx.ipc_status != IPC_OK) {
      if (dest->size() == expected)
        break;
      continue;
    }

    size_t res;
    memcpy(&res, e.payload, e.size);
    dest->insert(res);
    free(e.payload);
  }
}

void test_single_writer_single_reader() {
  const uint64_t size = ipc_channel_align_size(128);
  const size_t count = 200000;

  std::vector<uint8_t> mem(size);

  const IpcChannelResult channel_result =
      ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
  IpcChannel *channel = channel_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();

  std::thread producer(produce, channel, 0, count);
  std::thread consumer_thr(consume, channel, count, dest);

  producer.join();
  consumer_thr.join();

  assert(dest->size() == count);
  for (size_t i = 0; i < count; i++) {
    assert(dest->contains(i));
  }

  ipc_channel_destroy(channel);
}

void test_multiple_writer_single_reader() {
  const uint64_t size = ipc_channel_align_size(128);
  const size_t total = 300000;

  std::vector<uint8_t> mem(size);
  const IpcChannelResult channel_result =
      ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
  IpcChannel *channel = channel_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(produce, channel, 0, 100000);
  std::thread p2(produce, channel, 100000, 200000);
  std::thread p3(produce, channel, 200000, 300000);

  std::thread consumer_thr(consume, channel, total, dest);

  p1.join();
  p2.join();
  p3.join();
  consumer_thr.join();

  assert(dest->size() == total);
  for (size_t i = 0; i < total; i++) {
    assert(dest->contains(i));
  }

  ipc_channel_destroy(channel);
}

void test_multiple_writer_multiple_reader() {
  const uint64_t size = ipc_channel_align_size(128);
  const size_t total = 300000;

  std::vector<uint8_t> mem(size);
  const IpcChannelResult channel_result =
      ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
  IpcChannel *channel = channel_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(produce, channel, 0, 100000);
  std::thread p2(produce, channel, 100000, 200000);
  std::thread p3(produce, channel, 200000, 300000);

  std::thread c1(consume, channel, total, dest);
  std::thread c2(consume, channel, total, dest);
  std::thread c3(consume, channel, total, dest);

  p1.join();
  p2.join();
  p3.join();
  c1.join();
  c2.join();
  c3.join();

  assert(dest->size() == total);
  for (size_t i = 0; i < total; i++) {
    assert(dest->contains(i));
  }

  ipc_channel_destroy(channel);
}

void _test_race_between_skip_and_read() {
  const uint64_t size = ipc_channel_align_size(128);
  std::vector<uint8_t> mem(size);
  const IpcChannelResult channel_result =
      ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
  IpcChannel *channel = channel_result.result;

  const size_t val = 42;
  assert(ipc_channel_write(channel, &val, sizeof(val)).ipc_status == IPC_OK);

  IpcEntry entry;
  IpcChannelPeekResult pk = ipc_channel_peek(channel, &entry);
  assert(pk.ipc_status == IPC_OK);

  std::atomic<bool> skip_done = false;
  std::atomic<bool> read_done = false;

  IpcEntry e; // for reader

  std::thread t1([&] {
    IpcChannelSkipResult result = ipc_channel_skip(channel, entry.offset);
    skip_done.store(true);
    
    assert(result.ipc_status == IPC_OK || result.ipc_status == IPC_ERR_LOCKED ||
           result.ipc_status == IPC_ERR_OFFSET_MISMATCH ||
           result.ipc_status == IPC_EMPTY);
  });

  std::thread t2([&] {
    IpcChannelTryReadResult result = ipc_channel_try_read(channel, &e);
    read_done.store(true);
    if (result.ipc_status == IPC_OK) {
      size_t v;
      memcpy(&v, e.payload, e.size);
      assert(v == val);
      free(e.payload);
    } else {
      assert(result.ipc_status == IPC_EMPTY || result.ipc_status == IPC_ERR_LOCKED);
    }
  });

  t1.join();
  t2.join();
  assert(skip_done.load() && read_done.load());

  ipc_channel_destroy(channel);
}

void test_race_between_skip_and_read() {
  for (int i = 0; i < 1000; i++) {
    _test_race_between_skip_and_read();
  }
}

int main() {
  run_test("single writer & single reader", &test_single_writer_single_reader);
  run_test("multiple writer & single reader",
           &test_multiple_writer_single_reader);
  run_test("multiple writer & multiple reader",
           &test_multiple_writer_multiple_reader);

  run_test("race between skip and read", &test_race_between_skip_and_read);

  return 0;
}
