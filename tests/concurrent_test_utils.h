#pragma once

#include "concurrent_set.hpp"
#include "test_utils.h"
#include <thread>
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

void consume_buffer(IpcBuffer* buffer, size_t expected, 
                   std::shared_ptr<concurrent_set<size_t>> dest) {
    test_utils::EntryWrapper entry(sizeof(size_t));
    
    while (true) {
        if (dest->size() == expected) {
            break;
        }

        IpcEntry entry_ref = entry.get();
        IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
        if (result.ipc_status == IPC_OK) {
            size_t res;
            memcpy(&res, entry_ref.payload, entry_ref.size);
            dest->insert(res);
        }
    }
}

void consume_channel(IpcChannel* channel, size_t expected,
                     std::shared_ptr<concurrent_set<size_t>> dest) {
    IpcEntry entry;
    while (true) {
        if (dest->size() == expected) {
            break;
        }

        IpcChannelReadResult rx = ipc_channel_read(channel, &entry);
        if (rx.ipc_status != IPC_OK) {
            if (dest->size() == expected) {
                break;
            }
            continue;
        }

        size_t res;
        memcpy(&res, entry.payload, entry.size);
        dest->insert(res);
        free(entry.payload);
    }
}

template<typename ProducerFunc, typename ConsumerFunc>
void run_single_writer_single_reader_test(ProducerFunc producer, ConsumerFunc consumer,
                                         size_t count, size_t buffer_size) {
    test_utils::BufferWrapper buffer(buffer_size);
    auto dest = std::make_shared<concurrent_set<size_t>>();

    std::thread producer_thread(producer, buffer.get(), 0, count);
    std::thread consumer_thread(consumer, buffer.get(), count, dest);

    producer_thread.join();
    consumer_thread.join();

    CHECK(dest->size() == count);
    for (size_t i = 0; i < count; i++) {
        CHECK(dest->contains(i));
    }
}

template<typename ProducerFunc, typename ConsumerFunc>
void run_multiple_writer_single_reader_test(ProducerFunc producer, ConsumerFunc consumer,
                                           size_t total, size_t buffer_size) {
    test_utils::BufferWrapper buffer(buffer_size);
    auto dest = std::make_shared<concurrent_set<size_t>>();

    std::thread p1(producer, buffer.get(), 0, total / 3);
    std::thread p2(producer, buffer.get(), total / 3, 2 * total / 3);
    std::thread p3(producer, buffer.get(), 2 * total / 3, total);

    std::thread consumer_thread(consumer, buffer.get(), total, dest);

    p1.join();
    p2.join();
    p3.join();
    consumer_thread.join();

    CHECK(dest->size() == total);
    for (size_t i = 0; i < total; i++) {
        CHECK(dest->contains(i));
    }
}

template<typename ProducerFunc, typename ConsumerFunc>
void run_multiple_writer_multiple_reader_test(ProducerFunc producer, ConsumerFunc consumer,
                                             size_t total, size_t buffer_size) {
    test_utils::BufferWrapper buffer(buffer_size);
    auto dest = std::make_shared<concurrent_set<size_t>>();

    std::thread p1(producer, buffer.get(), 0, total / 3);
    std::thread p2(producer, buffer.get(), total / 3, 2 * total / 3);
    std::thread p3(producer, buffer.get(), 2 * total / 3, total);

    std::thread c1(consumer, buffer.get(), total, dest);
    std::thread c2(consumer, buffer.get(), total, dest);
    std::thread c3(consumer, buffer.get(), total, dest);

    p1.join();
    p2.join();
    p3.join();
    c1.join();
    c2.join();
    c3.join();

    CHECK(dest->size() == total);
    for (size_t i = 0; i < total; i++) {
        CHECK(dest->contains(i));
    }
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
            bool valid_status = (result.ipc_status == IPC_ERR_LOCKED || result.ipc_status == IPC_ERR_OFFSET_MISMATCH);
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
            bool valid_status = (result.ipc_status == IPC_EMPTY || result.ipc_status == IPC_ERR_LOCKED);
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
                            result.ipc_status == IPC_ERR_LOCKED ||
                            result.ipc_status == IPC_ERR_OFFSET_MISMATCH ||
                            result.ipc_status == IPC_EMPTY);
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
            bool valid_status = (result.ipc_status == IPC_EMPTY || result.ipc_status == IPC_ERR_LOCKED);
            CHECK(valid_status);
        }
    });

    t1.join();
    t2.join();
    CHECK(skip_done.load());
    CHECK(read_done.load());
}

} 
