#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "concurrency_manager.hpp"
#include "concurrent_test_utils.h"
#include "shmipc/ipc_channel.h"
#include "test_utils.h"
#include "unsafe_collector.hpp"
#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

TEST_CASE("single writer single reader") {

  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_init(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

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

  ipc_channel_detach(channel, nullptr);
}

TEST_CASE("single writer single reader with timeout") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_init(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  UnsafeCollector<size_t> collector;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_channel, channel, 0,
                       test_utils::DEFAULT_COUNT);

  const timespec timeout = {.tv_sec = 0, .tv_nsec = 10 * 1000000};
  manager.add_consumer(concurrent_test_utils::consume_channel_with_timeout,
                       channel, std::ref(collector),
                       std::ref(manager.get_manager()), &timeout);

  manager.run_and_wait();

  auto collected = collector.get_all_collected();
  CHECK(collected.size() == test_utils::DEFAULT_COUNT);
  for (size_t i = 0; i < test_utils::DEFAULT_COUNT; i++) {
    CHECK(collected.contains(i));
  }

  ipc_channel_detach(channel, nullptr);
}

TEST_CASE("multiple writer single reader") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_init(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

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

  ipc_channel_detach(channel, nullptr);
}

TEST_CASE("multiple writer multiple reader stress") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);
  const size_t total = 500000;

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_init(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

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

  ipc_entry_t entry;
  const ipc_status_t try_read_status = ipc_channel_try_read(channel, &entry, nullptr);
  CHECK(try_read_status == IPC_STATUS_EMPTY);

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

  ipc_channel_detach(channel, nullptr);
}

TEST_CASE("extreme stress test - small buffer") {
  for (int i = 0; i < 5; i++) {
    const uint64_t size =
        ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
        ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

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

    ipc_entry_t entry;
    const ipc_status_t try_read_status = ipc_channel_try_read(channel, &entry, nullptr);
    CHECK(try_read_status == IPC_STATUS_EMPTY);

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

    ipc_channel_detach(channel, nullptr);
  }
}

TEST_CASE("blocks reader until writer writes") {
  const uint64_t size = ipc_channel_suggest_size(test_utils::SMALL_BUFFER_SIZE);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_init(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  std::atomic<bool> reader_ready{false};

  std::thread writer([&]() {
    while (!reader_ready.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    test_utils::write_data(channel, 42);
  });

  ipc_entry_t entry;
  struct timespec timeout = {.tv_sec = 2000, .tv_nsec = 0};

  reader_ready.store(true, std::memory_order_release);

  const ipc_status_t read_status =
      ipc_channel_read(channel, &entry, &timeout, nullptr);
  CHECK(read_status == IPC_STATUS_OK);

  int value;
  memcpy(&value, entry.payload, sizeof(value));
  CHECK(value == 42);
  free(entry.payload);

  writer.join();

  ipc_channel_detach(channel, nullptr);
}
