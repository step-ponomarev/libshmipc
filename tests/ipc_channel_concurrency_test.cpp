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
    for (int i = 0; i < 10; i++) {
        concurrent_test_utils::run_multiple_writer_multiple_reader_channel_test(
            test_utils::LARGE_COUNT,
            test_utils::SMALL_BUFFER_SIZE
        );
    }
}

TEST_CASE("extreme channel stress test") {
    // Экстремальная нагрузка на канал для сравнения с буфером
    for (int i = 0; i < 20; i++) {
        concurrent_test_utils::run_multiple_writer_multiple_reader_channel_test(
            test_utils::LARGE_COUNT * 2,  // В 2 раза больше записей
            test_utils::SMALL_BUFFER_SIZE  // Маленький буфер для максимальной конкуренции
        );
    }
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
