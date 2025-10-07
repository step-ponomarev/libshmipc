#pragma once

#include "doctest/doctest.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "unsafe_collector.hpp"
#include "concurrency_manager.hpp"
#include "test_utils.h"
#include <stdbool.h>
#include <memory>

namespace concurrent_test_utils {

void produce_buffer(IpcBuffer* buffer, size_t from, size_t to) {
    for (size_t i = from; i < to;) {
        IpcBufferWriteResult status = ipc_buffer_write(buffer, &i, sizeof(size_t));
        if (status.ipc_status != IPC_OK) {
            continue;
        }
        i++;
    }
}

void delayed_produce_buffer(IpcBuffer* buffer, size_t from, size_t to) {
    for (size_t i = from; i < to;) {
        void* dest;
        IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer, sizeof(i), &dest);
        if (result.ipc_status != IPC_OK) {
            continue;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));
        memcpy(dest, &i, sizeof(i));
        ipc_buffer_commit_entry(buffer, result.result);
        i++;
    }
}

void produce_channel(IpcChannel* channel, size_t from, size_t to) {
    for (size_t i = from; i < to;) {
        IpcChannelWriteResult status = ipc_channel_write(channel, &i, sizeof(size_t));
        if (status.ipc_status != IPC_OK) {
            continue;
        }
        i++;
    }
}

void consume_buffer(IpcBuffer *buffer,
                     UnsafeCollector<size_t> &collector,
                     ConcurrencyManager<size_t>& manager) {
  test_utils::EntryWrapper entry(sizeof(size_t));
  IpcEntry entry_ref = entry.get();

  bool finished = false;
  while (true) {
    finished = manager.all_producers_finished();
    IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
    if (result.ipc_status == IPC_OK) {
      size_t res;
      memcpy(&res, entry_ref.payload, entry_ref.size);
      collector.collect(res);
    } else if (finished && result.ipc_status == IPC_EMPTY) {
      break;
    }
  }
}

void consume_channel(IpcChannel *channel,
                     UnsafeCollector<size_t> &collector,
                     ConcurrencyManager<size_t>& manager) {
  IpcEntry entry;
  bool finished = false;
  while (true) {
    finished = manager.all_producers_finished();
    IpcChannelTryReadResult result = ipc_channel_try_read(channel, &entry);
    if (result.ipc_status == IPC_OK) {
      size_t res;
      memcpy(&res, entry.payload, entry.size);
      collector.collect(res);
      free(entry.payload);
    } else if (finished && result.ipc_status == IPC_EMPTY) {
      break;
    }
  }
}

template<typename ProducerFunc, typename ConsumerFunc>
void run_single_writer_single_reader_test(ProducerFunc producer, ConsumerFunc consumer,
                                         size_t count, size_t buffer_size) {
    test_utils::BufferWrapper buffer(buffer_size);
    UnsafeCollector<size_t> collector;
    ConcurrencyManager<size_t> manager;

    manager.add_producer(producer, buffer.get(), 0, count);
    manager.add_consumer(consumer, buffer.get(), std::ref(collector), std::ref(manager.get_manager()));

    manager.run_and_wait();

    auto collected = collector.get_all_collected();
    CHECK(collected.size() == count);
    for (size_t i = 0; i < count; i++) {
        CHECK(collected.contains(i));
    }
}

template<typename ProducerFunc, typename ConsumerFunc>
void run_multiple_writer_single_reader_test(ProducerFunc producer, ConsumerFunc consumer,
                                           size_t total, size_t buffer_size) {
    test_utils::BufferWrapper buffer(buffer_size);
    UnsafeCollector<size_t> collector;
    ConcurrencyManager<size_t> manager;

    // Добавляем продюсеров
    manager.add_producer(producer, buffer.get(), 0, total / 3);
    manager.add_producer(producer, buffer.get(), total / 3, 2 * total / 3);
    manager.add_producer(producer, buffer.get(), 2 * total / 3, total);

    // Добавляем консьюмера
    manager.add_consumer(consumer, buffer.get(), std::ref(collector), std::ref(manager.get_manager()));

    // Запускаем и ждем завершения
    manager.run_and_wait();
    
    auto collected = collector.get_all_collected();
    CHECK(collected.size() == total);
    for (size_t i = 0; i < total; i++) {
        CHECK(collected.contains(i));
    }
}

template<typename ProducerFunc, typename ConsumerFunc>
void run_multiple_writer_multiple_reader_test(ProducerFunc producer, ConsumerFunc consumer,
                                             size_t buffer_size) {
    const size_t total = test_utils::LARGE_COUNT;
    test_utils::BufferWrapper buffer(buffer_size);
    UnsafeCollector<size_t> collector1, collector2, collector3;
    ConcurrencyManager<size_t> manager;

    manager.add_producer(producer, buffer.get(), 0, total / 3);
    manager.add_producer(producer, buffer.get(), total / 3, 2 * total / 3);
    manager.add_producer(producer, buffer.get(), 2 * total / 3, total);

    manager.add_consumer(consumer, buffer.get(), std::ref(collector1), std::ref(manager.get_manager()));
    manager.add_consumer(consumer, buffer.get(), std::ref(collector2), std::ref(manager.get_manager()));
    manager.add_consumer(consumer, buffer.get(), std::ref(collector3), std::ref(manager.get_manager()));

    // Запускаем и ждем завершения
    manager.run_and_wait();
    
    // Объединяем результаты всех коллекторов
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

// Функции для каналов
void run_single_writer_single_reader_channel_test(size_t count, size_t buffer_size) {
    const uint64_t size = ipc_channel_align_size(buffer_size);
    std::vector<uint8_t> mem(size);
    const IpcChannelResult channel_result = ipc_channel_create(mem.data(), size, test_utils::DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;
    
    UnsafeCollector<size_t> collector;
    ConcurrencyManager<size_t> manager;

    // Добавляем продюсера
    manager.add_producer(produce_channel, channel, 0, count);
    
    // Добавляем консьюмера
    manager.add_consumer(consume_channel, channel, std::ref(collector), std::ref(manager.get_manager()));

    // Запускаем и ждем завершения
    manager.run_and_wait();

    auto collected = collector.get_all_collected();
    CHECK(collected.size() == count);
    for (size_t i = 0; i < count; i++) {
        CHECK(collected.contains(i));
    }
    
    ipc_channel_destroy(channel);
}

void run_multiple_writer_single_reader_channel_test(size_t total, size_t buffer_size) {
    const uint64_t size = ipc_channel_align_size(buffer_size);
    std::vector<uint8_t> mem(size);
    const IpcChannelResult channel_result = ipc_channel_create(mem.data(), size, test_utils::DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;
    
    UnsafeCollector<size_t> collector;
    ConcurrencyManager<size_t> manager;

    // Добавляем продюсеров
    manager.add_producer(produce_channel, channel, 0, total / 3);
    manager.add_producer(produce_channel, channel, total / 3, 2 * total / 3);
    manager.add_producer(produce_channel, channel, 2 * total / 3, total);

    // Добавляем консьюмера
    manager.add_consumer(consume_channel, channel, std::ref(collector), std::ref(manager.get_manager()));

    // Запускаем и ждем завершения
    manager.run_and_wait();

    auto collected = collector.get_all_collected();
    CHECK(collected.size() == total);
    for (size_t i = 0; i < total; i++) {
        CHECK(collected.contains(i));
    }
    
    ipc_channel_destroy(channel);
}

void run_multiple_writer_multiple_reader_channel_test(size_t total, size_t buffer_size) {
    const uint64_t size = ipc_channel_align_size(buffer_size);
    std::vector<uint8_t> mem(size);
    const IpcChannelResult channel_result = ipc_channel_create(mem.data(), size, test_utils::DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;
    
    UnsafeCollector<size_t> collector1, collector2, collector3;
    ConcurrencyManager<size_t> manager;

    // Добавляем продюсеров
    manager.add_producer(produce_channel, channel, 0, total / 3);
    manager.add_producer(produce_channel, channel, total / 3, 2 * total / 3);
    manager.add_producer(produce_channel, channel, 2 * total / 3, total);

    // Добавляем консьюмеров с отдельными коллекторами
    manager.add_consumer(consume_channel, channel, std::ref(collector1), std::ref(manager.get_manager()));
    manager.add_consumer(consume_channel, channel, std::ref(collector2), std::ref(manager.get_manager()));
    manager.add_consumer(consume_channel, channel, std::ref(collector3), std::ref(manager.get_manager()));

    // Запускаем и ждем завершения
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

void test_race_between_skip_and_read_buffer() {
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

void test_race_between_skip_and_read_channel() {
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
        IpcChannelSkipResult result = ipc_channel_skip(channel.get(), entry.offset);
        skip_done.store(true);
        
        bool valid_status = (result.ipc_status == IPC_OK || 
                            result.ipc_status == IPC_ERR_OFFSET_MISMATCH ||
                            result.ipc_status == IPC_EMPTY 
                            || result.ipc_status == IPC_ERR_LOCKED);
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
            bool valid_status = (result.ipc_status == IPC_EMPTY);
            CHECK(valid_status);
        }
    });

    t1.join();
    t2.join();
    CHECK(skip_done.load());
    CHECK(read_done.load());
}
}
