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
