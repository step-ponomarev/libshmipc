#include "concurrent_set.hpp"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "test_runner.h"
#include <assert.h>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <string.h>
#include <thread>

static const IpcChannelConfiguration DEFAULT_CONFIG = {
    .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

void produce(IpcChannel *channel, const size_t from, const size_t to) {
  for (size_t i = from; i < to;) {
    IpcStatus status = ipc_channel_write(channel, &i, sizeof(size_t));
    if (status != IPC_OK) {
      continue;
    }

    i++;
  }
}

void consume(IpcChannel *channel, const size_t expected,
             std::shared_ptr<concurrent_set<size_t>> dest) {
  IpcEntry e;
  while (true) {
    if (dest->size() == expected) {
      break;
    }

    IpcTransaction tx = ipc_channel_read(channel, &e);
    if (tx.status != IPC_OK) {
      if (dest->size() == expected) {
        break;
      }

      continue;
    }

    size_t res;
    memcpy(&res, e.payload, e.size);
    dest->insert(res);

    free(e.payload);
  }
}

void test_single_writer_single_reader() {
  const uint64_t size = ipc_channel_allign_size(128);
  const size_t count = 200000;

  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  auto dest = std::make_shared<concurrent_set<size_t>>();

  std::thread producer(produce, channel, 0, count);
  std::thread consumer(consume, channel, count, dest);

  producer.join();
  consumer.join();

  assert(dest->size() == count);
  for (size_t i = 0; i < count; i++) {
    assert(dest->contains(i));
  }

  free(channel);
}

void test_multiple_writer_single_reader() {
  const uint64_t size = ipc_channel_allign_size(128);
  const size_t total = 300000;

  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(produce, channel, 0, 100000);
  std::thread p2(produce, channel, 100000, 200000);
  std::thread p3(produce, channel, 200000, 300000);

  std::thread consumer(consume, channel, total, dest);

  p1.join();
  p2.join();
  p3.join();
  consumer.join();
  assert(dest->size() == total);
  for (size_t i = 0; i < total; i++) {
    assert(dest->contains(i));
  }

  free(channel);
}

void test_multiple_writer_multiple_reader() {
  const uint64_t size = ipc_channel_allign_size(128);
  const size_t total = 300000;

  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(produce, channel, 0, 100000);
  std::thread p2(produce, channel, 100000, 200000);
  std::thread p3(produce, channel, 200000, 300000);

  std::thread consumer(consume, channel, total, dest);
  std::thread consumer2(consume, channel, total, dest);
  std::thread consumer3(consume, channel, total, dest);

  p1.join();
  p2.join();
  p3.join();
  consumer.join();
  consumer2.join();
  consumer3.join();

  assert(dest->size() == total);
  for (size_t i = 0; i < total; i++) {
    assert(dest->contains(i));
  }

  free(channel);
}

void test_race_between_skip_and_read() {
  const uint64_t size = ipc_channel_allign_size(128);
  uint8_t mem[size];
  IpcChannel *channel = ipc_channel_create(mem, size, DEFAULT_CONFIG);

  const size_t val = 42;
  assert(ipc_channel_write(channel, &val, sizeof(val)) == IPC_OK);

  IpcEntry entry;
  IpcTransaction tx = ipc_channel_peek(channel, &entry);
  assert(tx.status == IPC_OK);

  std::atomic<bool> skip_done = false;
  std::atomic<bool> read_done = false;

  std::thread t1([&] {
    IpcTransaction result = ipc_channel_skip(channel, tx.entry_id);
    skip_done = true;
    assert(result.status == IPC_OK || result.status == IPC_ALREADY_SKIPED);
  });

  std::thread t2([&] {
    IpcEntry e = {.payload = malloc(sizeof(size_t)), .size = sizeof(size_t)};
    IpcTransaction tx = ipc_channel_try_read(channel, &e);
    read_done = true;
    if (tx.status == IPC_OK) {
      size_t v;
      memcpy(&v, e.payload, e.size);
      assert(v == val);
    } else {
      assert(tx.status == IPC_EMPTY || tx.status == IPC_LOCKED);
    }
    free(e.payload);
  });

  t1.join();
  t2.join();
  assert(skip_done && read_done);
  free(channel);
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
