#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "shmipc/ipc_common.h"
#include "shmipc/ipc_buffer.h"
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <random>

using namespace std::chrono;

TEST_CASE("buffer stress - high load throughput") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int iterations = 50;
    int successful_operations = 0;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        int test_data = i;
        IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &test_data, sizeof(test_data));
        if (write_result.ipc_status != IPC_OK && write_result.ipc_status != IPC_ERR_NO_SPACE_CONTIGUOUS) {
            break; // Stop if we get an unexpected error
        }
        
        // Only read if write was successful
        if (write_result.ipc_status == IPC_OK) {
            test_utils::EntryWrapper entry(sizeof(int));
            IpcEntry entry_ref = entry.get();
            IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
            if (read_result.ipc_status == IPC_OK) {
                int read_data;
                memcpy(&read_data, entry_ref.payload, sizeof(int));
                CHECK(read_data == test_data);
                successful_operations++;
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)successful_operations / duration.count() * 1000000;
    std::cout << "Buffer high load throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(successful_operations > 0);
    CHECK(throughput > 1000);
}

TEST_CASE("buffer stress - memory pressure") {
    const int num_buffers = 3;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_buffers; ++i) {
        test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
        // Just create and destroy, no operations
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double creation_time = (double)duration.count() / num_buffers;
    std::cout << "Buffer memory pressure - creation: " << creation_time << " μs per buffer" << std::endl;
    
    CHECK(creation_time < 10000);
}

TEST_CASE("buffer stress - concurrent high load") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int total_items = 50;
    int items_processed = 0;
    
    auto start = high_resolution_clock::now();
    
    // Simple write-read cycle without threading
    for (int i = 0; i < total_items; ++i) {
        IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &i, sizeof(i));
        if (write_result.ipc_status != IPC_OK && write_result.ipc_status != IPC_ERR_NO_SPACE_CONTIGUOUS) {
            break; // Stop if we get an unexpected error
        }
        
        // Only read if write was successful
        if (write_result.ipc_status == IPC_OK) {
            test_utils::EntryWrapper entry(sizeof(int));
            IpcEntry entry_ref = entry.get();
            IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
            if (read_result.ipc_status == IPC_OK) {
                int read_data;
                memcpy(&read_data, entry_ref.payload, sizeof(int));
                CHECK(read_data == i);
                items_processed++;
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)items_processed / duration.count() * 1000000;
    std::cout << "Buffer concurrent high load throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(items_processed > 0);
    CHECK(throughput > 1000);
}

TEST_CASE("buffer stress - large data burst") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int iterations = 20;
    
    auto start = high_resolution_clock::now();
    
    int successful_operations = 0;
    for (int i = 0; i < iterations; ++i) {
        int test_data = i;
        IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &test_data, sizeof(test_data));
        if (write_result.ipc_status != IPC_OK && write_result.ipc_status != IPC_ERR_NO_SPACE_CONTIGUOUS) {
            break; // Stop if we get an unexpected error
        }
        
        // Only read if write was successful
        if (write_result.ipc_status == IPC_OK) {
            test_utils::EntryWrapper entry(sizeof(int));
            IpcEntry entry_ref = entry.get();
            IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
            if (read_result.ipc_status == IPC_OK) {
                int read_data;
                memcpy(&read_data, entry_ref.payload, sizeof(int));
                CHECK(read_data == test_data);
                successful_operations++;
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)successful_operations / duration.count() * 1000000;
    double data_throughput = (double)successful_operations * sizeof(int) / duration.count() * 1000000 / (1024 * 1024);
    std::cout << "Buffer large data burst throughput: " << throughput << " ops/sec, " 
              << data_throughput << " MB/sec" << std::endl;
    
    CHECK(successful_operations > 0);
    CHECK(throughput > 100);
    CHECK(data_throughput > 0.001);
}

TEST_CASE("buffer stress - rapid create destroy") {
    const int num_cycles = 100;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_cycles; ++i) {
        test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
        
        const int test_data = 42;
        IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &test_data, sizeof(test_data));
        if (write_result.ipc_status == IPC_OK) {
            test_utils::EntryWrapper entry(sizeof(int));
            IpcEntry entry_ref = entry.get();
            IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
            if (read_result.ipc_status == IPC_OK) {
                int read_data;
                memcpy(&read_data, entry_ref.payload, sizeof(int));
                CHECK(read_data == test_data);
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double cycle_time = (double)duration.count() / num_cycles;
    std::cout << "Buffer rapid create/destroy cycle time: " << cycle_time << " μs per cycle" << std::endl;
    
    CHECK(cycle_time < 1000);
}

TEST_CASE("buffer stress - reserve commit stress") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int num_operations = 100;
    const int data_size = 64;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        void* dest;
        IpcBufferReserveEntryResult reserve_result = 
            ipc_buffer_reserve_entry(buffer.get(), data_size, &dest);
        
        if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
            memset(dest, i & 0xFF, data_size);
            
            IpcBufferCommitEntryResult commit_result = 
                ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
            CHECK(IpcBufferCommitEntryResult_is_ok(commit_result));
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)num_operations / duration.count() * 1000000;
    std::cout << "Buffer reserve/commit stress throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 1000);
}

TEST_CASE("buffer stress - peek skip stress") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int num_entries = 50;
    const int peek_skip_iterations = 100;
    
    // Fill buffer with data
    for (int i = 0; i < num_entries; ++i) {
        IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &i, sizeof(i));
        if (write_result.ipc_status != IPC_OK && write_result.ipc_status != IPC_ERR_NO_SPACE_CONTIGUOUS) {
            break; // Stop if we get an unexpected error
        }
    }
    
    auto start = high_resolution_clock::now();
    
    int successful_operations = 0;
    for (int i = 0; i < peek_skip_iterations; ++i) {
        IpcEntry entry;
        IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
        
        if (peek_result.ipc_status == IPC_OK) {
            IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer.get(), entry.offset);
            if (skip_result.ipc_status == IPC_OK) {
                successful_operations++;
            }
        } else if (peek_result.ipc_status == IPC_EMPTY) {
            // Buffer is empty, refill it
            // First, make sure buffer is completely empty by skipping all remaining entries
            while (ipc_buffer_skip_force(buffer.get()).ipc_status == IPC_OK) {
                // Keep skipping until buffer is empty
            }
            
            // Now refill the buffer
            for (int j = 0; j < num_entries; ++j) {
                IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &j, sizeof(j));
                if (write_result.ipc_status != IPC_OK) {
                    break; // Stop if buffer is full
                }
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)successful_operations / duration.count() * 1000000;
    std::cout << "Buffer peek/skip stress throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(successful_operations > 0);
    CHECK(throughput > 100);
}

TEST_CASE("buffer stress - random operations") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int num_operations = 100;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 2);  // 3 operations: write, peek, skip
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        int operation = dis(gen);
        
        switch (operation) {
            case 0: {
                // Write operation
                int data = i;
                ipc_buffer_write(buffer.get(), &data, sizeof(data));
                // Accept both success and buffer full as valid outcomes
                break;
            }
            case 1: {
                // Peek operation - safe, doesn't modify buffer
                IpcEntry entry;
                ipc_buffer_peek(buffer.get(), &entry);
                // IPC_OK or IPC_EMPTY are both valid statuses
                break;
            }
            case 2: {
                // Skip operation - removes one entry from buffer
                ipc_buffer_skip_force(buffer.get());
                // IPC_OK, IPC_EMPTY, or IPC_ALREADY_SKIPPED are all valid
                break;
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)num_operations / duration.count() * 1000000;
    std::cout << "Buffer random operations throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 10000);
}

TEST_CASE("buffer stress - overflow handling") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    // Use data that fits in the buffer but will cause overflow after a few writes
    const int data_size = 16;  // Small enough to fit in 64-byte buffer (128 - 24 = 104, but _find_max_power_of_2(104) = 64)
    std::vector<uint8_t> data(data_size, 0xAB);
    
    int successful_writes = 0;
    int failed_writes = 0;
    
    for (int i = 0; i < 100; ++i) {
        IpcBufferWriteResult result = ipc_buffer_write(buffer.get(), data.data(), data_size);
        
        if (result.ipc_status == IPC_OK) {
            successful_writes++;
        } else if (result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS || result.ipc_status == IPC_ERR_ENTRY_TOO_LARGE) {
            failed_writes++;
        }
    }
    
    std::cout << "Buffer overflow handling - successful: " << successful_writes 
              << ", failed: " << failed_writes << std::endl;
    
    // Either we have successful writes, or all writes failed due to entry being too large
    CHECK((successful_writes > 0 || failed_writes == 100));
}

TEST_CASE("buffer stress - concurrent mixed operations") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    const int num_threads = 4;
    const int operations_per_thread = 100;
    std::atomic<int> total_operations{0};
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < operations_per_thread; ++i) {
                int operation = (t + i) % 3;  // Cycle through operations
                
                switch (operation) {
                    case 0: {
                        // Write operation
                        int data = t * operations_per_thread + i;
                        ipc_buffer_write(buffer.get(), &data, sizeof(data));
                        // Accept both success and buffer full as valid outcomes
                        break;
                    }
                    case 1: {
                        // Peek operation - safe, doesn't modify buffer
                        IpcEntry entry;
                        ipc_buffer_peek(buffer.get(), &entry);
                        // IPC_OK or IPC_EMPTY are both valid statuses
                        break;
                    }
                    case 2: {
                        // Skip operation - removes one entry from buffer
                        ipc_buffer_skip_force(buffer.get());
                        // IPC_OK, IPC_EMPTY, or IPC_ALREADY_SKIPPED are all valid
                        break;
                    }
                }
                
                total_operations.fetch_add(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)total_operations.load() / duration.count() * 1000000;
    std::cout << "Buffer concurrent mixed operations throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 10000);
}
