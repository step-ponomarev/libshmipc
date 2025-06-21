#include "concurrent_set.hpp"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "test_runner.h"
#include <assert.h>
#include <cstdint>
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
  IpcEntry e = {.size = sizeof(size_t), .payload = malloc(sizeof(size_t))};
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
  }

  free(e.payload);
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

int main() {
  run_test("single writer & single reader", &test_single_writer_single_reader);

  return 0;
}
