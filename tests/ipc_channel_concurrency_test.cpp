#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest/doctest.h"
#include "concurrent_test_utils.h"


TEST_CASE("single writer single reader") {
    concurrent_test_utils::run_single_writer_single_reader_channel_test(
        test_utils::DEFAULT_COUNT,
        test_utils::SMALL_BUFFER_SIZE
    );
}

TEST_CASE("multiple writer single reader") {
    concurrent_test_utils::run_multiple_writer_single_reader_channel_test(
        test_utils::LARGE_COUNT,
        test_utils::SMALL_BUFFER_SIZE
    );
}



TEST_CASE("multiple writer multiple reader stress") {
    concurrent_test_utils::run_multiple_writer_multiple_reader_channel_test(
        test_utils::LARGE_COUNT,
        test_utils::SMALL_BUFFER_SIZE
    );
}





TEST_CASE("race between skip and read") {
    for (int i = 0; i < 1000; i++) {
        concurrent_test_utils::test_race_between_skip_and_read_channel();
    }
}

TEST_CASE("extreme stress test - small buffer") {
    // Используем маленький буфер для создания максимальной конкуренции
    for (int i = 0; i < 5; i++) {
        concurrent_test_utils::run_multiple_writer_multiple_reader_channel_test(
            test_utils::LARGE_COUNT,
            test_utils::SMALL_BUFFER_SIZE  // Используем константу для маленького буфера
        );
    }
}
