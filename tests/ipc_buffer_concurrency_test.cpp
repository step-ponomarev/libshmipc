#include "concurrent_set.hpp"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_common.h"
#include "test_runner.h"
#include <assert.h>
#include <cstdint>
#include <stddef.h>
#include <string.h>
#include <thread>

void produce(IpcBuffer *buf, const size_t count) {
  for (size_t i = 0; i < count;) {
    if (ipc_buffer_write(buf, &i, sizeof(i)) != IPC_OK) {
      continue;
    }

    i++;
  }
}

void consume(IpcBuffer *buf, const size_t expected,
             std::shared_ptr<concurrent_set<size_t>> dest) {
  IpcEntry e = {.size = sizeof(size_t), .payload = malloc(sizeof(size_t))};

  size_t count = 0;
  while (true) {
    IpcTransaction tx = ipc_buffer_read(buf, &e);
    if (tx.status == IPC_EMPTY || tx.status == IPC_NOT_READY) {
      continue;
    }

    if (tx.status != IPC_OK) {
      break;
    }

    size_t res;
    memcpy(&res, e.payload, e.size);
    dest->insert(res);
    count++;

    if (count == expected) {
      break;
    }
  }

  free(e.payload);
}

void test_single_writer_single_reader() {
  const uint64_t size = ipc_buffer_allign_size(1024);
  const size_t count = 2000;

  uint8_t mem[size];
  IpcBuffer *buf = ipc_buffer_create(mem, size);

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread producer(produce, buf, count);
  std::thread consumer(consume, buf, count, dest);

  producer.join();
  consumer.join();

  assert(dest->size() == count);
  for (size_t i = 0; i < count; i++) {
    assert(dest->contains(i));
  }

  free(buf);
}

int main() {
  run_test("single writer & single reader", &test_single_writer_single_reader);
  return 0;
}
