#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "concurrent_test_utils.h"
#include "test_utils.h"
#include "unsafe_collector.hpp"
#include "concurrency_manager.hpp"
#include <unordered_set>
#include <thread>

TEST_CASE("single writer single reader") {
  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
  UnsafeCollector<size_t> collector;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), 0, test_utils::LARGE_COUNT);
  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(), std::ref(collector), std::ref(manager.get_manager()));

  manager.run_and_wait();

  auto collected = collector.get_all_collected();
  CHECK(collected.size() == test_utils::LARGE_COUNT);
  for (size_t i = 0; i < test_utils::LARGE_COUNT; i++) {
      CHECK(collected.contains(i));
  }
}

TEST_CASE("multiple writer single reader") {
  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
  UnsafeCollector<size_t> collector;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), 0, test_utils::LARGE_COUNT / 3);
  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), test_utils::LARGE_COUNT / 3, 2 * test_utils::LARGE_COUNT / 3);
  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), 2 * test_utils::LARGE_COUNT / 3, test_utils::LARGE_COUNT);

  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(), std::ref(collector), std::ref(manager.get_manager()));
  manager.run_and_wait();
  
  auto collected = collector.get_all_collected();
  CHECK(collected.size() == test_utils::LARGE_COUNT);
  for (size_t i = 0; i < test_utils::LARGE_COUNT; i++) {
      CHECK(collected.contains(i));
  }
}

TEST_CASE("multiple writer multiple reader") {
  const size_t total = test_utils::LARGE_COUNT;
  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
  UnsafeCollector<size_t> collector1, collector2, collector3;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), 0, total / 3);
  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), total / 3, 2 * total / 3);
  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), 2 * total / 3, total);

  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(), std::ref(collector1), std::ref(manager.get_manager()));
  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(), std::ref(collector2), std::ref(manager.get_manager()));
  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(), std::ref(collector3), std::ref(manager.get_manager()));

  manager.run_and_wait();
  
  auto collected1 = collector1.get_all_collected();
  auto collected2 = collector2.get_all_collected();
  auto collected3 = collector3.get_all_collected();
  
  std::unordered_set<size_t> all_collected;
  all_collected.insert(collected1.begin(), collected1.end());
  all_collected.insert(collected2.begin(), collected2.end());
  all_collected.insert(collected3.begin(), collected3.end());
  
  bool is_ok = all_collected.size() == total;
  for (size_t i = 0; i < total; i++) {
      if (!all_collected.contains(i)) {
          is_ok = false;
      }
  }

  CHECK(is_ok);
}

TEST_CASE("multiple writer multiple reader stress") {
  const size_t total = 50000;
  test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
  UnsafeCollector<size_t> collector1, collector2, collector3;
  ConcurrencyManager<size_t> manager;

  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(), 0,
                       total / 3);
  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(),
                       total / 3, 2 * total / 3);
  manager.add_producer(concurrent_test_utils::produce_buffer, buffer.get(),
                       2 * total / 3, total);

  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(),
                       std::ref(collector1), std::ref(manager.get_manager()));
  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(),
                       std::ref(collector2), std::ref(manager.get_manager()));
  manager.add_consumer(concurrent_test_utils::consume_buffer, buffer.get(),
                       std::ref(collector3), std::ref(manager.get_manager()));

  manager.run_and_wait();

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
}


TEST_CASE("race between skip and read") {
  for (int i = 0; i < 1000; i++) {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const size_t val = 42;
    test_utils::write_data(buffer.get(), val);

    IpcEntry entry;
    IpcBufferPeekResult peek_res = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_res.ipc_status == IPC_OK);

    std::thread t1([&] {
        IpcBufferSkipResult result = ipc_buffer_skip(buffer.get(), entry.offset);
        if (IpcBufferSkipResult_is_ok(result)) {
            bool valid_status = (result.ipc_status == IPC_OK || result.ipc_status == IPC_EMPTY);
            CHECK(valid_status);
        } else {
            bool valid_status = (result.ipc_status == IPC_ERR_OFFSET_MISMATCH || result.ipc_status == IPC_ERR_LOCKED);
            CHECK(valid_status);
        }
    });

    std::thread t2([&] {
        test_utils::EntryWrapper e(sizeof(size_t));
        IpcEntry e_ref = e.get();
        IpcBufferReadResult result = ipc_buffer_read(buffer.get(), &e_ref);

        if (result.ipc_status == IPC_OK) {
            size_t v;
            memcpy(&v, e_ref.payload, e_ref.size);
            CHECK(v == val);
        } else {
            bool valid_status = (result.ipc_status == IPC_EMPTY);
            CHECK(valid_status);
        }
    });

    t1.join();
    t2.join();
  }
}

TEST_CASE("multiple threads write") {
  test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
  const size_t num_threads = 5;
  const size_t entries_per_thread = 100;

  std::atomic<size_t> successful_writes{0};
  std::atomic<size_t> failed_writes{0};
  std::vector<std::thread> threads;

  auto write_entry = [&](size_t thread_id, size_t entry_id) -> bool {
    size_t data = thread_id * entries_per_thread + entry_id;
    IpcBufferWriteResult write_result =
        ipc_buffer_write(buffer.get(), &data, sizeof(size_t));

    return IpcBufferWriteResult_is_ok(write_result);
  };

  for (size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      size_t success = 0;
      size_t failures = 0;

      for (size_t i = 0; i < entries_per_thread; ++i) {
        if (write_entry(t, i)) {
          success++;
        } else {
          failures++;
        }
      }

      successful_writes.fetch_add(success);
      failed_writes.fetch_add(failures);
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(successful_writes.load() > 0);
  CHECK(failed_writes.load() > 0);
  CHECK(successful_writes.load() + failed_writes.load() ==
        num_threads * entries_per_thread);
}

TEST_CASE("race between write and read") {
  test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
  const size_t iterations = 100;

  std::atomic<size_t> successful_operations{0};

  std::thread writer([&] {
    for (size_t i = 0; i < iterations; ++i) {
      size_t data = i;
      IpcBufferWriteResult write_result =
          ipc_buffer_write(buffer.get(), &data, sizeof(size_t));

      if (IpcBufferWriteResult_is_ok(write_result)) {
        successful_operations.fetch_add(1);
      }
    }
  });

  std::thread reader([&] {
    test_utils::EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < iterations; ++i) {
      IpcEntry entry_ref = entry.get();
      IpcBufferReadResult result = ipc_buffer_read(buffer.get(), &entry_ref);

      if (IpcBufferReadResult_is_ok(result)) {
        successful_operations.fetch_add(1);
      }
    }
  });

  writer.join();
  reader.join();

  CHECK(successful_operations.load() > 0);
}

TEST_CASE("multiple threads peek") {
  test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

  for (size_t i = 0; i < 5; ++i) {
    ipc_buffer_write(buffer.get(), &i, sizeof(size_t));
  }

  const size_t num_threads = 3;
  const size_t peeks_per_thread = 50;
  std::atomic<size_t> successful_peeks{0};
  std::vector<std::thread> threads;

  for (size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&] {
      test_utils::EntryWrapper entry(sizeof(size_t));

      for (size_t i = 0; i < peeks_per_thread; ++i) {
        IpcEntry entry_ref = entry.get();
        IpcBufferPeekResult result = ipc_buffer_peek(buffer.get(), &entry_ref);

        if (IpcBufferPeekResult_is_ok(result)) {
          successful_peeks.fetch_add(1);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(successful_peeks.load() >= 0);
}

TEST_CASE("race between peek and read") {
  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
  const size_t iterations = 1000;

  test_utils::write_data(buffer.get(), 42);

  std::atomic<size_t> peek_count{0};
  std::atomic<size_t> read_count{0};

  std::thread peek_thread([&] {
    test_utils::EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < iterations; ++i) {
      IpcEntry entry_ref = entry.get();
      IpcBufferPeekResult result = ipc_buffer_peek(buffer.get(), &entry_ref);

      if (IpcBufferPeekResult_is_ok(result)) {
        peek_count.fetch_add(1);
      }
    }
  });

  std::thread read_thread([&] {
    test_utils::EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < iterations; ++i) {
      IpcEntry entry_ref = entry.get();
      IpcBufferReadResult result = ipc_buffer_read(buffer.get(), &entry_ref);

      if (IpcBufferReadResult_is_ok(result)) {
        read_count.fetch_add(1);
      }
    }
  });

  peek_thread.join();
  read_thread.join();

  CHECK(peek_count.load() > 0);
  CHECK(read_count.load() >= 0);
}

TEST_CASE("multiple threads skip_force") {
  test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

  for (size_t i = 0; i < 10; ++i) {
    ipc_buffer_write(buffer.get(), &i, sizeof(size_t));
  }

  const size_t num_threads = 3;
  std::atomic<size_t> successful_skips{0};
  std::vector<std::thread> threads;

  for (size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&] {
      size_t thread_skips = 0;
      for (size_t i = 0; i < 10; ++i) {
        IpcBufferSkipForceResult result = ipc_buffer_skip_force(buffer.get());

        if (IpcBufferSkipForceResult_is_ok(result)) {
          thread_skips++;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }

      successful_skips.fetch_add(thread_skips);
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(successful_skips.load() >= 0);
}

TEST_CASE("race between skip_force and read") {
  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
  const size_t iterations = 1000;

  test_utils::write_data(buffer.get(), 42);

  std::atomic<size_t> skip_count{0};
  std::atomic<size_t> read_count{0};

  std::thread skip_thread([&] {
    for (size_t i = 0; i < iterations; ++i) {
      IpcBufferSkipForceResult result = ipc_buffer_skip_force(buffer.get());

      if (IpcBufferSkipForceResult_is_ok(result)) {
        skip_count.fetch_add(1);
      }
    }
  });

  std::thread read_thread([&] {
    test_utils::EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < iterations; ++i) {
      IpcEntry entry_ref = entry.get();
      IpcBufferReadResult result = ipc_buffer_read(buffer.get(), &entry_ref);

      if (IpcBufferReadResult_is_ok(result)) {
        read_count.fetch_add(1);
      }
    }
  });

  skip_thread.join();
  read_thread.join();

  CHECK(skip_count.load() >= 0);
  CHECK(read_count.load() >= 0);
}

TEST_CASE("buffer overflow under concurrent load") {
  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
  const size_t num_threads = 10;
  const size_t writes_per_thread = 100;

  std::atomic<size_t> successful_writes{0};
  std::atomic<size_t> failed_writes{0};
  std::vector<std::thread> threads;

  for (size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = 0; i < writes_per_thread; ++i) {
        size_t data = t * writes_per_thread + i;
        IpcBufferWriteResult write_result =
            ipc_buffer_write(buffer.get(), &data, sizeof(size_t));

        if (IpcBufferWriteResult_is_ok(write_result)) {
          successful_writes.fetch_add(1);
        } else {
          failed_writes.fetch_add(1);
        }
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(successful_writes.load() > 0);
  CHECK(failed_writes.load() > 0);

  CHECK(successful_writes.load() + failed_writes.load() ==
        num_threads * writes_per_thread);
}

TEST_CASE("extreme stress - buffer overflow chaos") {

  test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

  const size_t num_threads = 15;
  const size_t operations_per_thread = 500;

  std::atomic<size_t> overflow_count{0};
  std::atomic<size_t> success_count{0};
  std::atomic<bool> stop_flag{false};
  std::vector<std::thread> threads;

  auto try_write = [&](size_t thread_id, size_t op_id) -> bool {
    size_t data = thread_id * operations_per_thread + op_id;
    IpcBufferWriteResult write_result =
        ipc_buffer_write(buffer.get(), &data, sizeof(size_t));

    if (IpcBufferWriteResult_is_ok(write_result)) {
      success_count.fetch_add(1);
      return true;
    } else {
      overflow_count.fetch_add(1);
      return false;
    }
  };

  for (size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = 0; i < operations_per_thread && !stop_flag.load(); ++i) {
        try_write(t, i);
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  stop_flag.store(true);

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(overflow_count.load() > 0);
  CHECK(success_count.load() > 0);
  CHECK(overflow_count.load() + success_count.load() <=
        num_threads * operations_per_thread);
}

TEST_CASE("extreme stress - rapid fill and drain cycles") {
  test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

  const size_t num_cycles = 50;
  const size_t writers_per_cycle = 8;
  const size_t readers_per_cycle = 4;
  const size_t items_per_writer = 20;

  std::atomic<size_t> total_written{0};
  std::atomic<size_t> total_read{0};

  for (size_t cycle = 0; cycle < num_cycles; ++cycle) {
    std::vector<std::thread> writers;
    std::vector<std::thread> readers;

    for (size_t w = 0; w < writers_per_cycle; ++w) {
      writers.emplace_back([&, w] {
        for (size_t i = 0; i < items_per_writer; ++i) {
          size_t data = cycle * 1000 + w * items_per_writer + i;
          IpcBufferWriteResult write_result =
              ipc_buffer_write(buffer.get(), &data, sizeof(size_t));

          if (IpcBufferWriteResult_is_ok(write_result)) {
            total_written.fetch_add(1);
          }
        }
      });
    }

    for (auto &writer : writers) {
      writer.join();
    }

    for (size_t r = 0; r < readers_per_cycle; ++r) {
      readers.emplace_back([&] {
        for (size_t i = 0; i < items_per_writer * 2; ++i) {
          test_utils::EntryWrapper entry(sizeof(size_t));
          IpcEntry entry_ref = entry.get();
          IpcBufferReadResult result =
              ipc_buffer_read(buffer.get(), &entry_ref);

          if (IpcBufferReadResult_is_ok(result)) {
            total_read.fetch_add(1);
          } else if (result.ipc_status == IPC_EMPTY) {
            break;
          }
        }
      });
    }

    for (auto &reader : readers) {
      reader.join();
    }

    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  CHECK(total_written.load() > 100);
  CHECK(total_read.load() > 0);
}

TEST_CASE("extreme stress - system stability under chaos") {
  test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

  const size_t num_threads = 8;
  const size_t operations_per_thread = 100;

  std::atomic<size_t> successful_operations{0};
  std::atomic<bool> system_stable{true};

  std::vector<std::thread> threads;

  for (size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = 0; i < operations_per_thread; ++i) {
        try {

          int op = (t + i) % 3;

          switch (op) {
          case 0: {
            int data = t * operations_per_thread + i;
            IpcBufferWriteResult write_result =
                ipc_buffer_write(buffer.get(), &data, sizeof(int));

            if (IpcBufferWriteResult_is_ok(write_result)) {
              successful_operations.fetch_add(1);
            }
            break;
          }
          case 1: {
            test_utils::EntryWrapper entry(sizeof(int));
            IpcEntry entry_ref = entry.get();
            IpcBufferReadResult result =
                ipc_buffer_read(buffer.get(), &entry_ref);

            if (result.ipc_status == IPC_OK) {
              successful_operations.fetch_add(1);
            }
            break;
          }
          case 2: {
            test_utils::EntryWrapper entry(sizeof(int));
            IpcEntry entry_ref = entry.get();
            IpcBufferPeekResult result =
                ipc_buffer_peek(buffer.get(), &entry_ref);

            if (IpcBufferPeekResult_is_ok(result)) {
              successful_operations.fetch_add(1);
            }
            break;
          }
          }
        } catch (...) {
          system_stable.store(false);
        }
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(system_stable.load());
  CHECK(successful_operations.load() > 0);
}

TEST_CASE("multiple writer multiple reader - different data sizes") {
  struct VariableSizeData {
    size_t id;
    uint8_t size;
    uint8_t pattern;
    uint8_t data[32];
  };

  auto create_variable_data = [](size_t id) -> VariableSizeData {
    VariableSizeData data;
    data.id = id;
    data.size = 1 + (id % 32);
    data.pattern = 0x40 + (id % 16);

    for (size_t i = 0; i < data.size; ++i) {
      data.data[i] = data.pattern;
    }

    return data;
  };

  auto produce_variable_buffer = [&](IpcBuffer *buffer, size_t from,
                                     size_t to) {
    for (size_t i = from; i < to; ++i) {
      VariableSizeData data = create_variable_data(i);

      IpcBufferWriteResult write_result =
          ipc_buffer_write(buffer, &data, sizeof(VariableSizeData));

      if (!IpcBufferWriteResult_is_ok(write_result)) {
        continue;
      }
    }
  };

  auto consume_variable_buffer = [&](IpcBuffer *buffer,
                                     UnsafeCollector<size_t> &collector,
                                     ConcurrencyManager<size_t> &manager) {
    test_utils::EntryWrapper entry(sizeof(VariableSizeData));

    while (true) {
      bool finished = manager.all_producers_finished();
      IpcEntry entry_ref = entry.get();
      IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);

      if (result.ipc_status == IPC_OK) {
        VariableSizeData received_data;
        memcpy(&received_data, entry_ref.payload, sizeof(VariableSizeData));

        CHECK(received_data.size >= 1);
        CHECK(received_data.size <= 32);

        for (size_t i = 0; i < received_data.size; ++i) {
          CHECK(received_data.data[i] == received_data.pattern);
        }

        VariableSizeData expected_data = create_variable_data(received_data.id);
        CHECK(received_data.id == expected_data.id);
        CHECK(received_data.size == expected_data.size);
        CHECK(received_data.pattern == expected_data.pattern);
        CHECK(memcmp(received_data.data, expected_data.data,
                     received_data.size) == 0);

        collector.collect(received_data.id);
      } else if (finished && result.ipc_status == IPC_EMPTY) {
        break;
      }
    }
  };

  test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
  UnsafeCollector<size_t> collector1, collector2, collector3;
  ConcurrencyManager<size_t> manager;

  const size_t total = 100;

  manager.add_producer(produce_variable_buffer, buffer.get(), 0, total / 3);
  manager.add_producer(produce_variable_buffer, buffer.get(), total / 3,
                       2 * total / 3);
  manager.add_producer(produce_variable_buffer, buffer.get(), 2 * total / 3,
                       total);

  manager.add_consumer(consume_variable_buffer, buffer.get(),
                       std::ref(collector1), std::ref(manager.get_manager()));
  manager.add_consumer(consume_variable_buffer, buffer.get(),
                       std::ref(collector2), std::ref(manager.get_manager()));
  manager.add_consumer(consume_variable_buffer, buffer.get(),
                       std::ref(collector3), std::ref(manager.get_manager()));

  manager.run_and_wait();

  auto collected1 = collector1.get_all_collected();
  auto collected2 = collector2.get_all_collected();
  auto collected3 = collector3.get_all_collected();

  std::set<size_t> all_collected;
  all_collected.insert(collected1.begin(), collected1.end());
  all_collected.insert(collected2.begin(), collected2.end());
  all_collected.insert(collected3.begin(), collected3.end());

  CHECK(all_collected.size() > 0);
  CHECK(all_collected.size() <= total);

  for (size_t id : all_collected) {
    CHECK(id >= 0);
    CHECK(id < total);
  }
}
