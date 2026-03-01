#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "shmipc/ipc_channel.h"
#include "test_utils.h"
#include <cstring>
#include <vector>

namespace {
    constexpr uint64_t DEFAULT_TIMEOUT_NS = 100000000;

    uint64_t timespec_to_nanos(const timespec *ts) {
        return (uint64_t) ts->tv_sec * 1000000000ULL + (uint64_t) ts->tv_nsec;
    }
}

TEST_CASE("channel init - null out") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("channel init - null memory pointer") {
    ipc_channel_t *channel = nullptr;
    ipc_error_t err;

    const uint64_t size = ipc_channel_suggest_size(128);
    const ipc_status_t status =
            ipc_channel_init(nullptr, size, &channel, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(channel == nullptr);
}

TEST_CASE("channel init - too small size") {
    uint8_t mem[16];

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const uint64_t small_size = 0;

    const ipc_status_t status =
            ipc_channel_init(mem, small_size, &channel, &err);
    const uint64_t min_size = ipc_channel_min_size();
    test_utils::CHECK_SIZE_ERROR(
        status, err, IPC_ERR_CODE_TOO_SMALL_SIZE, small_size, min_size, min_size);
    CHECK(channel == nullptr);
}

TEST_CASE("channel init - out is zeroed on error") {
    const uint64_t size = ipc_channel_suggest_size(128);

    ipc_channel_t *out = reinterpret_cast<ipc_channel_t *>(0xBAD);
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(nullptr, size, &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("channel init - error reset after failed then success") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;

    ipc_status_t status =
            ipc_channel_init(nullptr, size, &channel, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(channel == nullptr);

    status = ipc_channel_init(mem.data(), size, &channel, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(channel != nullptr);

    ipc_channel_detach(channel, nullptr);
}

TEST_CASE("channel init - success case") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

    ipc_channel_detach(channel, nullptr);
}

// ── attach ──

TEST_CASE("channel attach - null out") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_attach(mem.data(), nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("channel attach - null memory") {
    ipc_channel_t *channel = nullptr;
    ipc_error_t err;

    const ipc_status_t status =
            ipc_channel_attach(nullptr, &channel, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(channel == nullptr);
}

TEST_CASE("channel attach - out is null on error") {
    ipc_channel_t *out = reinterpret_cast<ipc_channel_t *>(0xBAD);
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_attach(nullptr, &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("channel attach - error reset after failed then success") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *created = nullptr;
    CHECK(ipc_channel_init(mem.data(), size, &created, nullptr) == IPC_STATUS_OK);

    ipc_channel_t *attached = nullptr;
    ipc_error_t err;
    ipc_status_t status = ipc_channel_attach(nullptr, &attached, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(attached == nullptr);

    status = ipc_channel_attach(mem.data(), &attached, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(attached != nullptr);

    ipc_channel_detach(created, nullptr);
    ipc_channel_detach(attached, nullptr);
}

TEST_CASE("channel attach - success case") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *created = nullptr;
    const ipc_status_t create_status =
            ipc_channel_init(mem.data(), size, &created, nullptr);
    CHECK(create_status == IPC_STATUS_OK);

    const int test_value = 42;
    CHECK(ipc_channel_write(created, &test_value, sizeof(test_value), nullptr) == IPC_STATUS_OK);

    ipc_channel_t *attached = nullptr;
    const ipc_status_t attach_status =
            ipc_channel_attach(mem.data(), &attached, nullptr);
    CHECK(attach_status == IPC_STATUS_OK);

    const int res = test_utils::read_data_safe<int>(attached, DEFAULT_TIMEOUT_NS);
    CHECK(res == test_value);

    ipc_channel_detach(created, nullptr);
    ipc_channel_detach(attached, nullptr);
}

// ── detach ──

TEST_CASE("channel detach - null channel") {
    ipc_error_t err;
    const ipc_status_t status = ipc_channel_detach(nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

// ── write ──

TEST_CASE("channel write - null channel") {
    const int test_data = 42;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_write(nullptr, &test_data, sizeof(test_data), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("channel write - null data") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_write(channel.get(), nullptr, sizeof(int), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("channel write - zero size") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_write(channel.get(), &test_data, 0, &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_ZERO_SIZE);
}

TEST_CASE("channel write - size exceeds buffer") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

    const size_t entry_size = sizeof(uint8_t) * 1024;
    void *payload = malloc(entry_size);
    ipc_error_t write_err;
    const ipc_status_t write_status =
            ipc_channel_write(channel, payload, entry_size, &write_err);
    CHECK(write_status == IPC_STATUS_ERROR);
    CHECK(write_err.kind == IPC_ERR_KIND_ARG);
    CHECK(write_err.code == IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER);
    CHECK(write_err.message != nullptr);
    CHECK(write_err.as.arg.size.requested_size > write_err.as.arg.size.limit);
    CHECK(write_err.as.arg.size.limit > 0);
    CHECK(write_err.as.arg.size.suggested_size == write_err.as.arg.size.limit);

    free(payload);
    ipc_channel_detach(channel, nullptr);
}

// ── read ──

TEST_CASE("channel write then read") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *producer = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &producer, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(producer != nullptr);

    const int val = 43;
    CHECK(ipc_channel_write(producer, &val, sizeof(val), nullptr) == IPC_STATUS_OK);

    ipc_channel_t *consumer = nullptr;
    const ipc_status_t attach_status =
            ipc_channel_attach(mem.data(), &consumer, nullptr);
    CHECK(attach_status == IPC_STATUS_OK);
    CHECK(consumer != nullptr);

    const int res = test_utils::read_data_safe<int>(consumer, DEFAULT_TIMEOUT_NS);
    CHECK(res == val);

    ipc_channel_detach(producer, nullptr);
    ipc_channel_detach(consumer, nullptr);
}

// ── try_read ──

TEST_CASE("channel try_read - success") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

    const int expected = 42;
    CHECK(ipc_channel_write(channel, &expected, sizeof(expected), nullptr) == IPC_STATUS_OK);

    ipc_entry_t entry;
    const ipc_status_t read_status = ipc_channel_try_read(channel, &entry, nullptr);
    CHECK(read_status == IPC_STATUS_OK);

    int res;
    memcpy(&res, entry.payload, entry.size);
    CHECK(expected == res);
    free(entry.payload);

    ipc_channel_detach(channel, nullptr);
}

TEST_CASE("channel try_read - empty") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

    ipc_entry_t entry;
    const ipc_status_t read_status = ipc_channel_try_read(channel, &entry, nullptr);
    CHECK(read_status == IPC_STATUS_EMPTY);

    ipc_channel_detach(channel, nullptr);
}

// ── read timeout ──

TEST_CASE("channel read - timeout") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

    const uint64_t timeout_ns = 1000000;

    timespec time;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
    const uint64_t before_ns = timespec_to_nanos(&time);

    ipc_entry_t entry;
    const ipc_status_t read_status =
            ipc_channel_read(channel, &entry, timeout_ns, nullptr);
    CHECK(read_status == IPC_STATUS_TIMEOUT);

    CHECK(clock_gettime(CLOCK_MONOTONIC, &time) == 0);

    const uint64_t after_ns = timespec_to_nanos(&time);
    CHECK(after_ns - before_ns >= timeout_ns);

    ipc_channel_detach(channel, nullptr);
}

// ── data tests ──

TEST_CASE("channel data - different sizes") {
    const uint64_t size = ipc_channel_suggest_size(2048);
    std::vector<uint8_t> mem(size);

    ipc_channel_t *channel = nullptr;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_channel_init(mem.data(), size, &channel, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(channel != nullptr);

    struct TestData {
        size_t size;
        uint8_t pattern;
    };

    std::vector<TestData> test_cases = {
        {1, 0xAA}, {4, 0xBB}, {8, 0xCC},
        {16, 0xDD}, {32, 0xEE}, {64, 0xFF},
        {128, 0x11}, {256, 0x22}
    };

    std::vector<std::vector<uint8_t> > written_data;
    for (const auto &test_case: test_cases) {
        std::vector<uint8_t> data(test_case.size, test_case.pattern);
        written_data.push_back(data);

        const ipc_status_t write_status =
                ipc_channel_write(channel, data.data(), data.size(), nullptr);

        if (write_status != IPC_STATUS_OK) {
            written_data.pop_back();
            break;
        }
    }

    for (size_t i = 0; i < written_data.size(); ++i) {
        ipc_entry_t entry;
        const ipc_status_t read_status =
                ipc_channel_read(channel, &entry, 100000000, nullptr);

        CHECK(read_status == IPC_STATUS_OK);
        CHECK(entry.size == written_data[i].size());
        CHECK(memcmp(entry.payload, written_data[i].data(),
            written_data[i].size()) == 0);

        free(entry.payload);
    }

    ipc_entry_t entry;
    const ipc_status_t try_read_status = ipc_channel_try_read(channel, &entry, nullptr);
    CHECK(try_read_status == IPC_STATUS_EMPTY);

    ipc_channel_detach(channel, nullptr);
}
