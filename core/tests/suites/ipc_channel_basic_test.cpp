#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "../src/ipc_utils.h"
#include "test_utils.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

static const IpcChannelConfiguration DEFAULT_CONFIG = {
    .max_round_trips = 1024, .start_sleep_ns = 1000, .max_sleep_ns = 100000};

TEST_CASE("invalid config") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelConfiguration config1 = {0, 10, 150};
    CHECK(IpcChannelOpenResult_is_error(ipc_channel_create(mem.data(), size, config1)));

    IpcChannelConfiguration config2 = {1, 0, 150};
    CHECK(IpcChannelOpenResult_is_error(ipc_channel_create(mem.data(), size, config2)));

    IpcChannelConfiguration config3 = {1, 1, 0};
    CHECK(IpcChannelOpenResult_is_error(ipc_channel_create(mem.data(), size, config3)));

    IpcChannelConfiguration config4 = {1, 100, 90};
    CHECK(IpcChannelOpenResult_is_error(ipc_channel_create(mem.data(), size, config4)));
}

TEST_CASE("write too large entry") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);
    const IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    const size_t entry_size = sizeof(uint8_t) * 1024;
    void *payload = malloc(entry_size);
    CHECK(ipc_channel_write(channel, payload, entry_size).ipc_status ==
          IPC_ERR_ENTRY_TOO_LARGE);

    free(payload);
    ipc_channel_destroy(channel);
}

TEST_CASE("write read") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result = ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *producer = channel_result.result;
    CHECK(producer != nullptr);

    const int val = 43;
    CHECK(ipc_channel_write(producer, &val, sizeof(val)).ipc_status == IPC_OK);

    IpcChannelConnectResult connect_result = ipc_channel_connect(mem.data(), DEFAULT_CONFIG);
    CHECK(IpcChannelConnectResult_is_ok(connect_result));
    IpcChannel *consumer = connect_result.result;
    CHECK(consumer != nullptr);

    const int res = test_utils::read_data_safe<int>(consumer);
    CHECK(res == val);

    ipc_channel_destroy(producer);
    ipc_channel_destroy(consumer);
}

TEST_CASE("destroy null") {
    CHECK(ipc_channel_destroy(nullptr).ipc_status == IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("peek") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);

    IpcChannel *channel = channel_result.result;

    const int expected = 42;
    CHECK(ipc_channel_write(channel, &expected, sizeof(expected)).ipc_status ==
          IPC_OK);

    IpcEntry entry;
    IpcChannelPeekResult pk = ipc_channel_peek(channel, &entry);
    CHECK(pk.ipc_status == IPC_OK);
    CHECK(entry.size == sizeof(expected));

    int peeked;
    memcpy(&peeked, entry.payload, sizeof(expected));
    CHECK(peeked == expected);

    IpcEntry entry2;
    IpcChannelReadResult rd = ipc_channel_read(channel, &entry2);
    CHECK(rd.ipc_status == IPC_OK);
    int read_val;
    memcpy(&read_val, entry2.payload, sizeof(read_val));
    CHECK(read_val == expected);
    free(entry2.payload);

    ipc_channel_destroy(channel);
}

TEST_CASE("peek empty") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);

    IpcChannel *channel = channel_result.result;

    IpcEntry entry;
    IpcChannelPeekResult pk = ipc_channel_peek(channel, &entry);
    CHECK(pk.ipc_status == IPC_EMPTY);

    ipc_channel_destroy(channel);
}

TEST_CASE("write try read") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    const int expected = 42;
    CHECK(ipc_channel_write(channel, &expected, sizeof(expected)).ipc_status ==
          IPC_OK);

    IpcEntry entry;
    CHECK(ipc_channel_try_read(channel, &entry).ipc_status == IPC_OK);

    int res;
    memcpy(&res, entry.payload, entry.size);
    CHECK(expected == res);
    free(entry.payload);

    ipc_channel_destroy(channel);
}

TEST_CASE("try read empty") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    IpcEntry entry;
    CHECK(ipc_channel_try_read(channel, &entry).ipc_status == IPC_EMPTY);

    ipc_channel_destroy(channel);
}

TEST_CASE("read retry limit reached") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    const int expected = -11;
    CHECK(ipc_channel_write(channel, &expected, sizeof(expected)).ipc_status == IPC_OK);

    IpcEntry peek_entry;
    IpcChannelPeekResult pk = ipc_channel_peek(channel, &peek_entry);
    CHECK(IpcChannelPeekResult_is_ok(pk));

    uint64_t* seq_ptr = (uint64_t*)((uint8_t*)peek_entry.payload - sizeof(uint64_t) * 3);
    uint64_t original_seq = *seq_ptr;
    *seq_ptr = 0xDEADBEEF;

    IpcEntry entry;
    CHECK(ipc_channel_read(channel, &entry).ipc_status == IPC_ERR_RETRY_LIMIT);

    *seq_ptr = original_seq;

    CHECK(ipc_channel_read(channel, &entry).ipc_status == IPC_OK);

    int res;
    memcpy(&res, entry.payload, sizeof(expected));
    CHECK(expected == res);
    free(entry.payload);

    ipc_channel_destroy(channel);
}

TEST_CASE("skip corrupted entry") {
    const uint64_t size = ipc_channel_suggest_size(256);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    const int first_val = 100;
    const int second_val = -11;

    CHECK(ipc_channel_write(channel, &first_val, sizeof(first_val)).ipc_status == IPC_OK);
    CHECK(ipc_channel_write(channel, &second_val, sizeof(second_val)).ipc_status == IPC_OK);

    IpcEntry peek_entry;
    IpcChannelPeekResult pk = ipc_channel_peek(channel, &peek_entry);
    CHECK(IpcChannelPeekResult_is_ok(pk));
    
    if (peek_entry.payload != nullptr && peek_entry.size >= sizeof(int)) {
        uint8_t* corrupt_ptr = (uint8_t*)peek_entry.payload - sizeof(uint64_t) * 3;
        *((uint64_t*)corrupt_ptr) = 0xDEADBEEF;
    }

    IpcEntry entry;
    IpcChannelReadResult read_res = ipc_channel_read(channel, &entry);
    CHECK(read_res.ipc_status == IPC_ERR_RETRY_LIMIT);

    pk = ipc_channel_peek(channel, &entry);
    CHECK(IpcChannelPeekResult_is_error(pk));

    CHECK(ipc_channel_skip_force(channel).ipc_status == IPC_OK);

    read_res = ipc_channel_read(channel, &entry);
    CHECK(read_res.ipc_status == IPC_OK);

    int res;
    memcpy(&res, entry.payload, sizeof(second_val));
    CHECK(second_val == res);
    free(entry.payload);

    ipc_channel_destroy(channel);
}

TEST_CASE("skip force") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);
    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    const int val = 42;
    CHECK(ipc_channel_write(channel, &val, sizeof(val)).ipc_status == IPC_OK);

    IpcEntry entry;
    CHECK(ipc_channel_peek(channel, &entry).ipc_status == IPC_OK);
    CHECK(ipc_channel_skip_force(channel).ipc_status == IPC_OK);
    CHECK(ipc_channel_peek(channel, &entry).ipc_status == IPC_EMPTY);

    ipc_channel_destroy(channel);
}

TEST_CASE("read timeout") {
    const uint64_t size = ipc_channel_suggest_size(128);
    std::vector<uint8_t> mem(size);

    IpcChannelOpenResult channel_result =
        ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;

    const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
    const uint64_t timeout_ns = ipc_timespec_to_nanos(&timeout);

    struct timespec time;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
    const uint64_t before_ns = ipc_timespec_to_nanos(&time);

    IpcEntry entry;
    CHECK(ipc_channel_read_with_timeout(channel, &entry, &timeout).ipc_status ==
          IPC_ERR_TIMEOUT);

    CHECK(clock_gettime(CLOCK_MONOTONIC, &time) == 0);

    const uint64_t after_ns = ipc_timespec_to_nanos(&time);
    CHECK(after_ns - before_ns >= timeout_ns);

    ipc_channel_destroy(channel);
}



TEST_CASE("channel data - different sizes") {
    const uint64_t size = ipc_channel_suggest_size(2048);
    std::vector<uint8_t> mem(size);
    
    IpcChannelOpenResult channel_result = ipc_channel_create(mem.data(), size, DEFAULT_CONFIG);
    IpcChannel *channel = channel_result.result;
    
    struct TestData {
        size_t size;
        uint8_t pattern;
    };
    
    std::vector<TestData> test_cases = {
        {1, 0xAA},
        {4, 0xBB},
        {8, 0xCC},
        {16, 0xDD},
        {32, 0xEE},
        {64, 0xFF},
        {128, 0x11},
        {256, 0x22}
    };
    
    std::vector<std::vector<uint8_t>> written_data;
    for (const auto& test_case : test_cases) {
        std::vector<uint8_t> data(test_case.size, test_case.pattern);
        written_data.push_back(data);
        
        IpcChannelWriteResult write_result = 
            ipc_channel_write(channel, data.data(), data.size());
        
        if (!IpcChannelWriteResult_is_ok(write_result)) {
            written_data.pop_back();
            break;
        }
    }
    
    for (size_t i = 0; i < written_data.size(); ++i) {
        IpcEntry entry;
        IpcChannelReadResult read_result = ipc_channel_read(channel, &entry);
        
        CHECK(read_result.ipc_status == IPC_OK);
        CHECK(entry.size == written_data[i].size());
        CHECK(memcmp(entry.payload, written_data[i].data(), written_data[i].size()) == 0);
        
        free(entry.payload);
    }
    
    IpcEntry entry;
    IpcChannelTryReadResult read_result = ipc_channel_try_read(channel, &entry);
    CHECK(read_result.ipc_status == IPC_EMPTY);
    
    ipc_channel_destroy(channel);
}
