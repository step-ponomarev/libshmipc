#include "concurrent_set.hpp"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_common.h"
#include "test_runner.h"
#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stddef.h>
#include <string.h>
#include <thread>
#include <vector>

void delayed_produce(IpcBuffer *buf, const size_t from, const size_t to) {
  for (size_t i = from; i < to;) {
    void *dest;
    IpcBufferReserveEntryResult result =
        ipc_buffer_reserve_entry(buf, sizeof(i), &dest);
    if (result.ipc_status != IPC_OK) {
      continue;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(10));
    memcpy(dest, &i, sizeof(i));
    ipc_buffer_commit_entry(buf, result.result);

    i++;
  }
}

void produce(IpcBuffer *buf, const size_t from, const size_t to) {
  for (size_t i = from; i < to;) {
    IpcBufferWriteResult status = ipc_buffer_write(buf, &i, sizeof(size_t));
    if (status.ipc_status != IPC_OK) {
      continue;
    }
    i++;
  }
}

void consume(IpcBuffer *buf, const size_t expected,
             std::shared_ptr<concurrent_set<size_t>> dest) {
  IpcEntry e;
  e.payload = malloc(sizeof(size_t));
  e.size = sizeof(size_t);

  while (true) {
    if (dest->size() == expected) {
      break;
    }

    IpcBufferReadResult result = ipc_buffer_read(buf, &e);
    if (IpcBufferReadResult_is_error(result)) {
      continue;
    }

    size_t res;
    memcpy(&res, e.payload, e.size);
    dest->insert(res);
  }

  free(e.payload);
}

void test_single_writer_single_reader() {
  const uint64_t size = ipc_buffer_align_size(128);
  const size_t count = 200000;

  std::vector<uint8_t> mem(size);
  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem.data(), size);
  IpcBuffer *buf = buffer_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();

  std::thread producer(produce, buf, 0, count);
  std::thread consumer(consume, buf, count, dest);

  producer.join();
  consumer.join();

  assert(dest->size() == count);
  for (size_t i = 0; i < count; i++) {
    assert(dest->contains(i));
  }

  free(buf);
}

void test_multiple_writer_single_reader() {
  const uint64_t size = ipc_buffer_align_size(128);
  const size_t total = 300000;

  std::vector<uint8_t> mem(size);
  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem.data(), size);
  IpcBuffer *buf = buffer_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(produce, buf, 0, 100000);
  std::thread p2(produce, buf, 100000, 200000);
  std::thread p3(produce, buf, 200000, 300000);

  std::thread consumer(consume, buf, total, dest);

  p1.join();
  p2.join();
  p3.join();
  consumer.join();
  assert(dest->size() == total);
  for (size_t i = 0; i < total; i++) {
    assert(dest->contains(i));
  }

  free(buf);
}

void test_multiple_writer_multiple_reader() {
  const uint64_t size = ipc_buffer_align_size(128);
  const size_t total = 300000;

  std::vector<uint8_t> mem(size);
  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem.data(), size);
  IpcBuffer *buf = buffer_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(produce, buf, 0, 100000);
  std::thread p2(produce, buf, 100000, 200000);
  std::thread p3(produce, buf, 200000, 300000);

  std::thread consumer(consume, buf, total, dest);
  std::thread consumer2(consume, buf, total, dest);
  std::thread consumer3(consume, buf, total, dest);

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

  free(buf);
}

void test_delayed_multiple_writer_multiple_reader() {
  const uint64_t size = ipc_buffer_align_size(128);
  const size_t total = 3000;

  std::vector<uint8_t> mem(size);
  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem.data(), size);
  IpcBuffer *buf = buffer_result.result;

  auto dest = std::make_shared<concurrent_set<size_t>>();
  std::thread p1(delayed_produce, buf, 0, 1000);
  std::thread p2(delayed_produce, buf, 1000, 2000);
  std::thread p3(delayed_produce, buf, 2000, 3000);

  std::thread consumer(consume, buf, total, dest);
  std::thread consumer2(consume, buf, total, dest);
  std::thread consumer3(consume, buf, total, dest);

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

  free(buf);
}

void _test_race_between_skip_and_read() {
  const uint64_t size = ipc_buffer_align_size(128);
  std::vector<uint8_t> mem(size);
  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mem.data(), size);
  IpcBuffer *buf = buffer_result.result;

  const size_t val = 42;
  assert(ipc_buffer_write(buf, &val, sizeof(val)).ipc_status == IPC_OK);

  IpcEntry entry;
  IpcBufferPeekResult peek_res = ipc_buffer_peek(buf, &entry);
  assert(peek_res.ipc_status == IPC_OK);

  std::thread t1([&] {
    IpcBufferSkipResult result = ipc_buffer_skip(buf, entry.offset);

    if (IpcBufferSkipResult_is_ok(result)) {
      assert(result.ipc_status == IPC_OK ||
             result.ipc_status == IPC_EMPTY);
    } else {
      assert(result.ipc_status == IPC_ERR_LOCKED ||
             result.ipc_status == IPC_ERR_OFFSET_MISMATCH);
    }
  });

  std::thread t2([&] {
    IpcEntry e;
    e.payload = malloc(sizeof(size_t));
    e.size = sizeof(size_t);
    IpcBufferReadResult result = ipc_buffer_read(buf, &e);

    if (result.ipc_status == IPC_OK) {
      size_t v;
      memcpy(&v, e.payload, e.size);
      assert(v == val);
    } else {
      assert(result.ipc_status == IPC_EMPTY || result.ipc_status == IPC_ERR_LOCKED);
    }
    free(e.payload);
  });

  t1.join();
  t2.join();
  free(buf);
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
  run_test("multiple delayed writer & multiple reader",
           &test_delayed_multiple_writer_multiple_reader);

  run_test("race between skip and read", &test_race_between_skip_and_read);

  

  return 0;
}
