#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "shmipc/ipc_common.h"
#include "shmipc/ipc_channel.h"
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>

using namespace std::chrono;

TEST_CASE("channel performance - basic throughput") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int iterations = 1000;
    const int test_data = 42;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        test_utils::write_data(channel.get(), test_data);
        const int read_data = test_utils::read_data<int>(channel.get());
        CHECK(read_data == test_data);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)iterations / duration.count() * 1000000;
    std::cout << "Basic throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 1000);
}

TEST_CASE("channel performance - peek operations") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    const int peek_iterations = 1000;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < peek_iterations; ++i) {
        IpcEntry entry;
        const IpcChannelPeekResult result = ipc_channel_peek(channel.get(), &entry);
        CHECK(result.ipc_status == IPC_OK);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)peek_iterations / duration.count() * 1000000;
    std::cout << "Peek operations throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 5000);
}

TEST_CASE("channel performance - skip operations") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelPeekResult peek_result = ipc_channel_peek(channel.get(), &entry);
    CHECK(peek_result.ipc_status == IPC_OK);
    
    const int skip_iterations = 100;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < skip_iterations; ++i) {
        const IpcChannelSkipResult result = ipc_channel_skip(channel.get(), entry.offset);
        CHECK(result.ipc_status == IPC_OK);
        
        test_utils::write_data(channel.get(), test_data);
        const IpcChannelPeekResult new_peek = ipc_channel_peek(channel.get(), &entry);
        CHECK(new_peek.ipc_status == IPC_OK);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)skip_iterations / duration.count() * 1000000;
    std::cout << "Skip operations throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(throughput > 100);
}

TEST_CASE("channel performance - large data") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int data_size = 256;
    const int iterations = 10;
    std::vector<uint8_t> large_data(data_size, 0xAB);
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        IpcChannelWriteResult write_result = 
            ipc_channel_write(channel.get(), large_data.data(), data_size);
        CHECK(write_result.ipc_status == IPC_OK);
        
        IpcEntry entry;
        const IpcChannelReadResult read_result = ipc_channel_read(channel.get(), &entry);
        CHECK(read_result.ipc_status == IPC_OK);
        CHECK(entry.size == data_size);
        free(entry.payload);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)iterations / duration.count() * 1000000;
    double data_throughput = (double)iterations * data_size / duration.count() * 1000000 / (1024 * 1024);
    std::cout << "Large data throughput: " << throughput << " ops/sec, " 
              << data_throughput << " MB/sec" << std::endl;
    
    CHECK(throughput > 1);
    CHECK(data_throughput > 0.001);
}

TEST_CASE("channel performance - concurrent operations") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const int total_items = 100;
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::atomic<bool> producer_done{false};
    
    auto start = high_resolution_clock::now();
    
    std::thread producer([&] {
        for (int i = 0; i < total_items; ++i) {
            IpcChannelWriteResult result = ipc_channel_write(channel.get(), &i, sizeof(i));
            if (result.ipc_status == IPC_OK) {
                items_produced.fetch_add(1);
            }
        }
        producer_done.store(true);
    });
    
    std::thread consumer([&] {
        while (!producer_done.load() || items_consumed.load() < items_produced.load()) {
            IpcEntry entry;
            IpcChannelReadResult result = ipc_channel_read(channel.get(), &entry);
            if (result.ipc_status == IPC_OK) {
                items_consumed.fetch_add(1);
                free(entry.payload);
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double throughput = (double)items_consumed.load() / duration.count() * 1000000;
    std::cout << "Concurrent throughput: " << throughput << " ops/sec" << std::endl;
    
    CHECK(items_consumed.load() > 0);
    CHECK(throughput > 100);
}

TEST_CASE("channel performance - memory usage") {
    const int num_channels = 10;
    std::vector<test_utils::ChannelWrapper> channels;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < num_channels; ++i) {
        channels.emplace_back(test_utils::SMALL_BUFFER_SIZE);
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    double creation_time = (double)duration.count() / num_channels;
    std::cout << "Channel creation time: " << creation_time << " μs per channel" << std::endl;
    
    CHECK(creation_time < 10000);
    
    for (auto& channel : channels) {
        const int test_data = 42;
        test_utils::write_data(channel.get(), test_data);
        const int read_data = test_utils::read_data<int>(channel.get());
        CHECK(read_data == test_data);
    }
}