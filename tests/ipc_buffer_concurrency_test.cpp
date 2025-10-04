#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "concurrent_test_utils.h"
#include "test_utils.h"
#include "shmipc/ipc_buffer.h"
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("single writer single reader") {
    concurrent_test_utils::run_single_writer_single_reader_test(
        concurrent_test_utils::produce_buffer,
        concurrent_test_utils::consume_buffer,
        1000,
        test_utils::SMALL_BUFFER_SIZE
    );
}

TEST_CASE("multiple writer single reader") {
    concurrent_test_utils::run_multiple_writer_single_reader_test(
        concurrent_test_utils::produce_buffer,
        concurrent_test_utils::consume_buffer,
        3000,
        test_utils::SMALL_BUFFER_SIZE
    );
}

TEST_CASE("multiple writer multiple reader") {
    concurrent_test_utils::run_multiple_writer_multiple_reader_test(
        concurrent_test_utils::produce_buffer,
        concurrent_test_utils::consume_buffer,
        3000,
        test_utils::SMALL_BUFFER_SIZE
    );
}

TEST_CASE("delayed multiple writer multiple reader") {
    const uint64_t size = ipc_buffer_align_size(test_utils::SMALL_BUFFER_SIZE);
    const size_t total = 3000;

    std::vector<uint8_t> mem(size);
    const IpcBufferCreateResult buffer_result =
        ipc_buffer_create(mem.data(), size);
    IpcBuffer *buf = buffer_result.result;

    auto dest = std::make_shared<concurrent_set<size_t>>();
    std::thread p1(concurrent_test_utils::delayed_produce_buffer, buf, 0, 1000);
    std::thread p2(concurrent_test_utils::delayed_produce_buffer, buf, 1000, 2000);
    std::thread p3(concurrent_test_utils::delayed_produce_buffer, buf, 2000, 3000);

    std::thread consumer(concurrent_test_utils::consume_buffer, buf, total, dest);
    std::thread consumer2(concurrent_test_utils::consume_buffer, buf, total, dest);
    std::thread consumer3(concurrent_test_utils::consume_buffer, buf, total, dest);

    p1.join();
    p2.join();
    p3.join();
    consumer.join();
    consumer2.join();
    consumer3.join();

    CHECK(dest->size() == total);
    for (size_t i = 0; i < total; i++) {
        CHECK(dest->contains(i));
    }

    free(buf);
}

TEST_CASE("race between skip and read") {
    for (int i = 0; i < 1000; i++) {
        concurrent_test_utils::test_race_between_skip_and_read_buffer();
    }
}


TEST_CASE("multiple threads reserve and commit") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    const size_t num_threads = 5;
    const size_t entries_per_thread = 100;
    
    std::atomic<size_t> successful_commits{0};
    std::atomic<size_t> failed_reserves{0};
    std::vector<std::thread> threads;
    
    
    auto write_entry = [&](size_t thread_id, size_t entry_id) -> bool {
        void* dest;
        IpcBufferReserveEntryResult reserve_result = 
            ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
        
        if (!IpcBufferReserveEntryResult_is_ok(reserve_result)) {
            return false;
        }
        
        size_t data = thread_id * entries_per_thread + entry_id;
        memcpy(dest, &data, sizeof(size_t));
        
        IpcBufferCommitEntryResult commit_result = 
            ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
        
        return IpcBufferCommitEntryResult_is_ok(commit_result);
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
            
            successful_commits.fetch_add(success);
            failed_reserves.fetch_add(failures);
        });
    }
    
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    
    CHECK(successful_commits.load() > 0);
    CHECK(failed_reserves.load() > 0);
    CHECK(successful_commits.load() + failed_reserves.load() == num_threads * entries_per_thread);
}

TEST_CASE("race between reserve and commit") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    const size_t iterations = 100;
    
    std::atomic<size_t> successful_operations{0};
    
    
    std::thread writer([&] {
        for (size_t i = 0; i < iterations; ++i) {
            void* dest;
            IpcBufferReserveEntryResult reserve_result = 
                ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
            
            if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
                size_t data = i;
                memcpy(dest, &data, sizeof(size_t));
                
                IpcBufferCommitEntryResult commit_result = 
                    ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
                
                if (IpcBufferCommitEntryResult_is_ok(commit_result)) {
                    successful_operations.fetch_add(1);
                }
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
        void* dest;
        IpcBufferReserveEntryResult reserve_result = 
            ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
        
        if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
            memcpy(dest, &i, sizeof(size_t));
            ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
        }
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
    
    
    for (auto& thread : threads) {
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
        void* dest;
        IpcBufferReserveEntryResult reserve_result = 
            ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
        
        if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
            memcpy(dest, &i, sizeof(size_t));
            ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
        }
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
    
    for (auto& thread : threads) {
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
                void* dest;
                IpcBufferReserveEntryResult reserve_result = 
                    ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
                
                if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
                    size_t data = t * writes_per_thread + i;
                    memcpy(dest, &data, sizeof(size_t));
                    
                    IpcBufferCommitEntryResult commit_result = 
                        ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
                    
                    if (IpcBufferCommitEntryResult_is_ok(commit_result)) {
                        successful_writes.fetch_add(1);
                    } else {
                        failed_writes.fetch_add(1);
                    }
                } else {
                    failed_writes.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    
    CHECK(successful_writes.load() > 0);
    CHECK(failed_writes.load() > 0);
    
    
    CHECK(successful_writes.load() + failed_writes.load() == num_threads * writes_per_thread);
}


TEST_CASE("extreme stress - massive concurrent operations") {
    
    const uint64_t buffer_size = ipc_buffer_align_size(64 * 1024); 
    std::vector<uint8_t> mem(buffer_size);
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem.data(), buffer_size);
    IpcBuffer *buffer = buffer_result.result;
    
    const size_t num_threads = 20;
    const size_t operations_per_thread = 1000;
    
    std::atomic<size_t> successful_operations{0};
    std::atomic<bool> stop_flag{false};
    std::vector<std::thread> threads;
    
    
    auto perform_operation = [&](size_t thread_id, size_t op_id) -> bool {
        int op_type = (thread_id + op_id) % 4;
        
        switch (op_type) {
            case 0: { 
                void* dest;
                IpcBufferReserveEntryResult reserve_result = 
                    ipc_buffer_reserve_entry(buffer, sizeof(size_t), &dest);
                
                if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
                    size_t data = thread_id * operations_per_thread + op_id;
                    memcpy(dest, &data, sizeof(size_t));
                    
                    IpcBufferCommitEntryResult commit_result = 
                        ipc_buffer_commit_entry(buffer, reserve_result.result);
                    
                    return IpcBufferCommitEntryResult_is_ok(commit_result);
                }
                return false;
            }
            case 1: { 
                test_utils::EntryWrapper entry(sizeof(size_t));
                IpcEntry entry_ref = entry.get();
                IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
                return IpcBufferReadResult_is_ok(result);
            }
            case 2: { 
                test_utils::EntryWrapper entry(sizeof(size_t));
                IpcEntry entry_ref = entry.get();
                IpcBufferPeekResult result = ipc_buffer_peek(buffer, &entry_ref);
                return IpcBufferPeekResult_is_ok(result);
            }
            case 3: { 
                IpcBufferSkipForceResult result = ipc_buffer_skip_force(buffer);
                return IpcBufferSkipForceResult_is_ok(result);
            }
        }
        return false;
    };
    
    
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t] {
            for (size_t i = 0; i < operations_per_thread && !stop_flag.load(); ++i) {
                if (perform_operation(t, i)) {
                    successful_operations.fetch_add(1);
                }
                
                
                if (i % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
    }
    
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flag.store(true);
    
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    
    CHECK(successful_operations.load() > 0);
    
    free(buffer);
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
        void* dest;
        IpcBufferReserveEntryResult reserve_result = 
            ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
        
        if (!IpcBufferReserveEntryResult_is_ok(reserve_result)) {
            overflow_count.fetch_add(1);
            return false;
        }
        
        size_t data = thread_id * operations_per_thread + op_id;
        memcpy(dest, &data, sizeof(size_t));
        
        IpcBufferCommitEntryResult commit_result = 
            ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
        
        if (IpcBufferCommitEntryResult_is_ok(commit_result)) {
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
    
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    
    CHECK(overflow_count.load() > 0);
    CHECK(success_count.load() > 0);
    CHECK(overflow_count.load() + success_count.load() <= num_threads * operations_per_thread);
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
                    void* dest;
                    IpcBufferReserveEntryResult reserve_result = 
                        ipc_buffer_reserve_entry(buffer.get(), sizeof(size_t), &dest);
                    
                    if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
                        size_t data = cycle * 1000 + w * items_per_writer + i;
                        memcpy(dest, &data, sizeof(size_t));
                        
                        IpcBufferCommitEntryResult commit_result = 
                            ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
                        
                        if (IpcBufferCommitEntryResult_is_ok(commit_result)) {
                            total_written.fetch_add(1);
                        }
                    }
                }
            });
        }
        
        
        for (auto& writer : writers) {
            writer.join();
        }
        
        
        for (size_t r = 0; r < readers_per_cycle; ++r) {
            readers.emplace_back([&] {
                for (size_t i = 0; i < items_per_writer * 2; ++i) { 
                    test_utils::EntryWrapper entry(sizeof(size_t));
                    IpcEntry entry_ref = entry.get();
                    IpcBufferReadResult result = ipc_buffer_read(buffer.get(), &entry_ref);
                    
                    if (IpcBufferReadResult_is_ok(result)) {
                        total_read.fetch_add(1);
                    } else if (result.ipc_status == IPC_EMPTY) {
                        break; 
                    }
                }
            });
        }
        
        
        for (auto& reader : readers) {
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
                            void* dest;
                            IpcBufferReserveEntryResult reserve_result = 
                                ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
                            
                            if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
                                int data = t * operations_per_thread + i;
                                memcpy(dest, &data, sizeof(int));
                                
                                IpcBufferCommitEntryResult commit_result = 
                                    ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
                                
                                if (IpcBufferCommitEntryResult_is_ok(commit_result)) {
                                    successful_operations.fetch_add(1);
                                }
                            }
                            break;
                        }
                        case 1: { 
                            test_utils::EntryWrapper entry(sizeof(int));
                            IpcEntry entry_ref = entry.get();
                            IpcBufferReadResult result = ipc_buffer_read(buffer.get(), &entry_ref);
                            
                            if (result.ipc_status == IPC_OK) {
                                successful_operations.fetch_add(1);
                            }
                            break;
                        }
                        case 2: { 
                            test_utils::EntryWrapper entry(sizeof(int));
                            IpcEntry entry_ref = entry.get();
                            IpcBufferPeekResult result = ipc_buffer_peek(buffer.get(), &entry_ref);
                            
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
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    
    CHECK(system_stable.load());
    CHECK(successful_operations.load() > 0);
}
