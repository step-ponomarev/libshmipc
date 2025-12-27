#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "concurrency_manager.hpp"
#include "concurrent_test_utils.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "test_utils.h"
#include "unsafe_collector.hpp"
#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

TEST_CASE("single writer single reader") {

  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);
  const IpcChannelOpenResult channel_result =
      ipc_channel_create(mem.data(), size);
  IpcChannel *channel = channel_result.result;

  UnsafeCollector<size_t> collector;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_channel, channel, 0,
                       test_utils::DEFAULT_COUNT);

  manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                       std::ref(collector), std::ref(manager.get_manager()));

  manager.run_and_wait();

  auto collected = collector.get_all_collected();
  CHECK(collected.size() == test_utils::DEFAULT_COUNT);
  for (size_t i = 0; i < test_utils::DEFAULT_COUNT; i++) {
    CHECK(collected.contains(i));
  }

  ipc_channel_destroy(channel);
}

TEST_CASE("multiple writer single reader") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);
  const IpcChannelOpenResult channel_result =
      ipc_channel_create(mem.data(), size);
  IpcChannel *channel = channel_result.result;

  UnsafeCollector<size_t> collector;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_channel, channel, 0,
                       test_utils::LARGE_COUNT / 3);
  manager.add_producer(concurrent_test_utils::produce_channel, channel,
                       test_utils::LARGE_COUNT / 3,
                       2 * test_utils::LARGE_COUNT / 3);
  manager.add_producer(concurrent_test_utils::produce_channel, channel,
                       2 * test_utils::LARGE_COUNT / 3,
                       test_utils::LARGE_COUNT);

  manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                       std::ref(collector), std::ref(manager.get_manager()));

  manager.run_and_wait();

  auto collected = collector.get_all_collected();
  CHECK(collected.size() == test_utils::LARGE_COUNT);
  for (size_t i = 0; i < test_utils::LARGE_COUNT; i++) {
    CHECK(collected.contains(i));
  }

  ipc_channel_destroy(channel);
}

TEST_CASE("multiple writer multiple reader stress") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);
  const IpcChannelOpenResult channel_result =
      ipc_channel_create(mem.data(), size);

  const size_t total = 500000;
  IpcChannel *channel = channel_result.result;

  UnsafeCollector<size_t> collector1, collector2, collector3;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_channel, channel, 0,
                       total / 3);
  manager.add_producer(concurrent_test_utils::produce_channel, channel,
                       total / 3, 2 * total / 3);
  manager.add_producer(concurrent_test_utils::produce_channel, channel,
                       2 * total / 3, total);

  manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                       std::ref(collector1), std::ref(manager.get_manager()));
  manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                       std::ref(collector2), std::ref(manager.get_manager()));
  manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                       std::ref(collector3), std::ref(manager.get_manager()));

  manager.run_and_wait();

  IpcEntry entry;
  IpcChannelPeekResult peek_res = ipc_channel_peek(channel, &entry);
  CHECK(peek_res.ipc_status == IPC_EMPTY);

  auto collected1 = collector1.get_all_collected();
  auto collected2 = collector2.get_all_collected();
  auto collected3 = collector3.get_all_collected();

  std::unordered_set<size_t> all_collected;
  all_collected.insert(collected1.begin(), collected1.end());
  all_collected.insert(collected2.begin(), collected2.end());
  all_collected.insert(collected3.begin(), collected3.end());

  CHECK(all_collected.size() == total);
  for (size_t i = 0; i < total; i++) {
    CHECK(all_collected.contains(i));
  }

  ipc_channel_destroy(channel);
}

TEST_CASE("race between skip and read") {
  for (int i = 0; i < 1000; i++) {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const size_t val = 42;
    test_utils::write_data(channel.get(), val);

    IpcEntry entry;
    IpcChannelPeekResult pk = ipc_channel_peek(channel.get(), &entry);
    CHECK(pk.ipc_status == IPC_OK);

    std::atomic<bool> skip_done = false;
    std::atomic<bool> read_done = false;

    IpcEntry e;

    std::thread t1([&] {
      IpcChannelSkipResult result =
          ipc_channel_skip(channel.get(), entry.offset);
      skip_done.store(true);

      bool valid_status = (result.ipc_status == IPC_OK ||
                           result.ipc_status == IPC_ERR_OFFSET_MISMATCH ||
                           result.ipc_status == IPC_EMPTY ||
                           result.ipc_status == IPC_ERR_LOCKED);
      CHECK(valid_status);
    });

    std::thread t2([&] {
      IpcChannelTryReadResult result = ipc_channel_try_read(channel.get(), &e);
      read_done.store(true);
      if (result.ipc_status == IPC_OK) {
        size_t v;
        memcpy(&v, e.payload, e.size);
        CHECK(v == val);
        free(e.payload);
      } else {
        bool valid_status =
            (result.ipc_status == IPC_OK || result.ipc_status == IPC_EMPTY ||
             result.ipc_status == IPC_ERR_LOCKED);
        CHECK(valid_status);
      }
    });

    t1.join();
    t2.join();
    CHECK(skip_done.load());
    CHECK(read_done.load());
  }
}

TEST_CASE("extreme stress test - small buffer") {
  for (int i = 0; i < 5; i++) {
    const uint64_t size =
        ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
    std::vector<uint8_t> mem(size);
    const IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size);
    IpcChannel *channel = channel_result.result;

    UnsafeCollector<size_t> collector1, collector2, collector3;
    ConcurrencyManager<size_t> manager;

    manager.add_producer(concurrent_test_utils::produce_channel, channel, 0,
                         test_utils::LARGE_COUNT / 3);
    manager.add_producer(concurrent_test_utils::produce_channel, channel,
                         test_utils::LARGE_COUNT / 3,
                         2 * test_utils::LARGE_COUNT / 3);
    manager.add_producer(concurrent_test_utils::produce_channel, channel,
                         2 * test_utils::LARGE_COUNT / 3,
                         test_utils::LARGE_COUNT);

    manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                         std::ref(collector1), std::ref(manager.get_manager()));
    manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                         std::ref(collector2), std::ref(manager.get_manager()));
    manager.add_consumer(concurrent_test_utils::consume_channel, channel,
                         std::ref(collector3), std::ref(manager.get_manager()));

    manager.run_and_wait();

    IpcEntry entry;
    IpcChannelPeekResult peek_res = ipc_channel_peek(channel, &entry);
    CHECK(peek_res.ipc_status == IPC_EMPTY);

    auto collected1 = collector1.get_all_collected();
    auto collected2 = collector2.get_all_collected();
    auto collected3 = collector3.get_all_collected();

    std::unordered_set<size_t> all_collected;
    all_collected.insert(collected1.begin(), collected1.end());
    all_collected.insert(collected2.begin(), collected2.end());
    all_collected.insert(collected3.begin(), collected3.end());

    CHECK(all_collected.size() == test_utils::LARGE_COUNT);
    for (size_t i = 0; i < test_utils::LARGE_COUNT; i++) {
      CHECK(all_collected.contains(i));
    }

    ipc_channel_destroy(channel);
  }
}

TEST_CASE("futex blocks reader until writer writes") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);
  const IpcChannelOpenResult channel_result =
      ipc_channel_create(mem.data(), size);
  IpcChannel *channel = channel_result.result;

  std::atomic<int> sequence{0};
  std::atomic<bool> reader_started{false};

  std::thread reader([&]() {
    reader_started = true;

    while (sequence.load(std::memory_order_acquire) < 1) {
      std::this_thread::yield();
    }

    CHECK(sequence.load(std::memory_order_acquire) == 1);
    IpcEntry entry;
    struct timespec timeout = {.tv_sec = 10,
                               .tv_nsec = 0}; // Long timeout for blocking test
    const IpcChannelReadResult result =
        ipc_channel_read(channel, &entry, &timeout);

    CHECK(result.ipc_status == IPC_OK);

    int value;
    memcpy(&value, entry.payload, sizeof(value));
    CHECK(value == 42);
    free(entry.payload);

    sequence.store(3, std::memory_order_release);
  });

  while (!reader_started.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  sequence.store(1, std::memory_order_release);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  test_utils::write_data(channel, 42);
  sequence.store(2, std::memory_order_release);

  reader.join();

  CHECK(sequence.load(std::memory_order_acquire) == 3);

  ipc_channel_destroy(channel);
}
