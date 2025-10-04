#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "shmipc/ipc_channel.h"
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>

using namespace std::chrono;

TEST_CASE("channel stress - high load throughput") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int iterations = 10000;
    const int test_data = 42;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        IpcChannelWriteResult write_result = ipc_channel_write(channel.get(), &test_data, sizeof(test_data));
        
        if (write_result.ipc_status == IPC_OK) {
            const int read_data = test_utils::read_data<int>(channel.get());
            CHECK(read_data == test_data);
        } else if (write_result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS) {
            // Expected overflow - channel is full
            std::cout << "Channel overflow at iteration " << i << std::endl;
            break;
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)iterations / duration.count() * 1000000;
    std::cout << "High load throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 100000);
}

TEST_CASE("channel stress - memory pressure") {
    const int num_channels = 10;
    std::vector<test_utils::ChannelWrapper> channels;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_channels; ++i) {
        channels.emplace_back(test_utils::SMALL_BUFFER_SIZE);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double creation_time = (double)duration.count() / num_channels;
    std::cout << "Memory pressure - channel creation: " << std::fixed << std::setprecision(1) << creation_time << " μs per channel" << std::endl;
    
    CHECK(creation_time < 1000);
    
    for (auto& channel : channels) {
        const int test_data = 42;
        test_utils::write_data(channel.get(), test_data);
        const int read_data = test_utils::read_data<int>(channel.get());
        CHECK(read_data == test_data);
    }
}

TEST_CASE("channel stress - concurrent high load") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int total_items = 50;
    const int num_producers = 2;
    const int num_consumers = 1;
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::atomic<bool> producer_done{false};
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < total_items / num_producers; ++i) {
                int data = p * (total_items / num_producers) + i;
                IpcChannelWriteResult result = ipc_channel_write(channel.get(), &data, sizeof(data));
                if (result.ipc_status == IPC_OK) {
                    items_produced.fetch_add(1);
                } else if (result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS) {
                    // Expected overflow - channel is full
                    break;
                }
            }
        });
    }
    
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&] {
            while (!producer_done.load() || items_consumed.load() < items_produced.load()) {
                IpcEntry entry;
                IpcChannelReadResult result = ipc_channel_read(channel.get(), &entry);
                if (result.ipc_status == IPC_OK) {
                    items_consumed.fetch_add(1);
                    free(entry.payload);
                } else if (result.ipc_status == IPC_EMPTY) {
                    // Channel is empty, wait a bit
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        });
    }
    
    for (auto& producer : producers) {
        producer.join();
    }
    producer_done.store(true);
    
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)items_consumed.load() / duration.count() * 1000000;
    std::cout << "Concurrent high load throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
    
    CHECK(items_consumed.load() > 0);
    CHECK(throughput > 5);
}

TEST_CASE("channel stress - large data burst") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int data_size = 512;
    const int iterations = 100;
    std::vector<uint8_t> large_data(data_size, 0xAB);
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        IpcChannelWriteResult write_result = 
            ipc_channel_write(channel.get(), large_data.data(), data_size);
        
        if (write_result.ipc_status == IPC_OK) {
            IpcEntry entry;
            const IpcChannelReadResult read_result = ipc_channel_read(channel.get(), &entry);
            CHECK(read_result.ipc_status == IPC_OK);
            CHECK(entry.size == data_size);
            free(entry.payload);
        } else if (write_result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS) {
            // Expected overflow - channel is full
            std::cout << "Channel overflow at iteration " << i << std::endl;
            break;
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)iterations / duration.count() * 1000000;
    double data_throughput = (double)iterations * data_size / duration.count() * 1000000 / (1024 * 1024);
    std::cout << "Large data burst throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec, " 
              << std::fixed << std::setprecision(1) << data_throughput << " MB/sec" << std::endl;
    
    CHECK(throughput > 100);
    CHECK(data_throughput > 1.0);
}

TEST_CASE("channel stress - rapid create destroy") {
    const int num_cycles = 100;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_cycles; ++i) {
        test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
        
        const int test_data = 42;
        IpcChannelWriteResult write_result = ipc_channel_write(channel.get(), &test_data, sizeof(test_data));
        
        if (write_result.ipc_status == IPC_OK) {
            const int read_data = test_utils::read_data<int>(channel.get());
            CHECK(read_data == test_data);
        } else if (write_result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS) {
            // Expected overflow - channel is full
            break;
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double cycle_time = (double)duration.count() / num_cycles;
    std::cout << "Rapid create/destroy cycle time: " << std::fixed << std::setprecision(2) << cycle_time << " μs per cycle" << std::endl;
    
    CHECK(cycle_time < 1000);
}

TEST_CASE("channel stress - timeout under load") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int timeout_iterations = 100;
    struct timespec timeout = {0, 1000};
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < timeout_iterations; ++i) {
        IpcEntry entry;
        IpcChannelReadWithTimeoutResult result = 
            ipc_channel_read_with_timeout(channel.get(), &entry, &timeout);
        
        if (result.ipc_status == IPC_ERR_TIMEOUT) {
            // Expected timeout - channel is empty
        } else if (result.ipc_status == IPC_OK) {
            free(entry.payload);
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)timeout_iterations / duration.count() * 1000000;
    std::cout << "Timeout under load throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 1000);
}

TEST_CASE("channel stress - random operations") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int num_operations = 100;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 3);
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        int operation = dis(gen);
        
        switch (operation) {
            case 0: {
                int data = i;
                IpcChannelWriteResult write_result = ipc_channel_write(channel.get(), &data, sizeof(data));
                if (write_result.ipc_status != IPC_OK && 
                    write_result.ipc_status != IPC_ERR_NO_SPACE_CONTIGUOUS && 
                    write_result.ipc_status != IPC_ERR_NO_SPACE_CONTIGUOUS) {
                    // Unexpected error
                    break;
                }
                break;
            }
            case 1: {
                IpcEntry entry;
                IpcChannelReadResult result = ipc_channel_read(channel.get(), &entry);
                if (result.ipc_status == IPC_OK) {
                    free(entry.payload);
                }
                // IPC_EMPTY is expected, no action needed
                break;
            }
            case 2: {
                IpcEntry entry;
                ipc_channel_peek(channel.get(), &entry);
                // Peek doesn't consume data
                break;
            }
            case 3: {
                IpcEntry entry;
                IpcChannelTryReadResult result = ipc_channel_try_read(channel.get(), &entry);
                if (result.ipc_status == IPC_OK) {
                    free(entry.payload);
                }
                // IPC_EMPTY is expected for try_read, no action needed
                break;
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)num_operations / duration.count() * 1000000;
    std::cout << "Random operations throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 10);
}
