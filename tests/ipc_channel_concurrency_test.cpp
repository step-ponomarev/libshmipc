#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "concurrent_test_utils.h"
#include "test_utils.h"
#include "concurrent_set.hpp"
#include "shmipc/ipc_channel.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("single writer single reader") {
    const uint64_t size = ipc_channel_align_size(test_utils::SMALL_BUFFER_SIZE);
    const size_t count = test_utils::DEFAULT_COUNT;

    std::vector<uint8_t> mem(size);

    const IpcChannelResult channel_result =
        ipc_channel_create(mem.data(), size, test_utils::DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    auto dest = std::make_shared<concurrent_set<size_t>>();

    std::thread producer(concurrent_test_utils::produce_channel, channel, 0, count);
    std::thread consumer_thr(concurrent_test_utils::consume_channel, channel, count, dest);

    producer.join();
    consumer_thr.join();

    CHECK(dest->size() == count);
    for (size_t i = 0; i < count; i++) {
        CHECK(dest->contains(i));
    }

    ipc_channel_destroy(channel);
}

TEST_CASE("multiple writer single reader") {
    const uint64_t size = ipc_channel_align_size(test_utils::SMALL_BUFFER_SIZE);
    const size_t total = test_utils::LARGE_COUNT;

    std::vector<uint8_t> mem(size);
    const IpcChannelResult channel_result =
        ipc_channel_create(mem.data(), size, test_utils::DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    auto dest = std::make_shared<concurrent_set<size_t>>();
    std::thread p1(concurrent_test_utils::produce_channel, channel, 0, total / 3);
    std::thread p2(concurrent_test_utils::produce_channel, channel, total / 3, 2 * total / 3);
    std::thread p3(concurrent_test_utils::produce_channel, channel, 2 * total / 3, total);

    std::thread consumer_thr(concurrent_test_utils::consume_channel, channel, total, dest);

    p1.join();
    p2.join();
    p3.join();
    consumer_thr.join();

    CHECK(dest->size() == total);
    for (size_t i = 0; i < total; i++) {
        CHECK(dest->contains(i));
    }

    ipc_channel_destroy(channel);
}

TEST_CASE("multiple writer multiple reader") {
    const uint64_t size = ipc_channel_align_size(test_utils::SMALL_BUFFER_SIZE);
    const size_t total = test_utils::LARGE_COUNT;

    std::vector<uint8_t> mem(size);
    const IpcChannelResult channel_result =
        ipc_channel_create(mem.data(), size, test_utils::DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    auto dest = std::make_shared<concurrent_set<size_t>>();
    std::thread p1(concurrent_test_utils::produce_channel, channel, 0, total / 3);
    std::thread p2(concurrent_test_utils::produce_channel, channel, total / 3, 2 * total / 3);
    std::thread p3(concurrent_test_utils::produce_channel, channel, 2 * total / 3, total);

    std::thread c1(concurrent_test_utils::consume_channel, channel, total, dest);
    std::thread c2(concurrent_test_utils::consume_channel, channel, total, dest);
    std::thread c3(concurrent_test_utils::consume_channel, channel, total, dest);

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

    ipc_channel_destroy(channel);
}

TEST_CASE("race between skip and read") {
    for (int i = 0; i < 1000; i++) {
        concurrent_test_utils::test_race_between_skip_and_read_channel();
    }
}
