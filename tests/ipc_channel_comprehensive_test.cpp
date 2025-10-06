#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "shmipc/ipc_channel.h"
#include <cstring>
#include <ctime>


TEST_CASE("channel align_size - valid alignment") {
    const uint64_t aligned_size = ipc_channel_align_size(0);
    CHECK(aligned_size > 0);
    
    const size_t test_sizes[] = {1, 10, 100, 1000, 10000};
    for (size_t size : test_sizes) {
        const uint64_t aligned = ipc_channel_align_size(size);
        CHECK(aligned >= size);
        CHECK(aligned > 0);
    }
}


TEST_CASE("channel create - NULL memory pointer") {
    const IpcChannelResult result = ipc_channel_create(nullptr, 128, test_utils::DEFAULT_CONFIG);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel create - too small size") {
    uint8_t mem[128];
    const IpcChannelResult result = ipc_channel_create(mem, 0, test_utils::DEFAULT_CONFIG);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel create - invalid config - zero max_round_trips") {
    uint8_t mem[128];
    IpcChannelConfiguration invalid_config = {0, 1000, 100000};
    const IpcChannelResult result = ipc_channel_create(mem, 128, invalid_config);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel create - invalid config - zero start_sleep_ns") {
    uint8_t mem[128];
    IpcChannelConfiguration invalid_config = {1024, 0, 100000};
    const IpcChannelResult result = ipc_channel_create(mem, 128, invalid_config);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel create - invalid config - zero max_sleep_ns") {
    uint8_t mem[128];
    IpcChannelConfiguration invalid_config = {1024, 1000, 0};
    const IpcChannelResult result = ipc_channel_create(mem, 128, invalid_config);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel create - invalid config - start_sleep_ns > max_sleep_ns") {
    uint8_t mem[test_utils::SMALL_BUFFER_SIZE];
    IpcChannelConfiguration invalid_config = {1024, 100000, 1000};
    const IpcChannelResult result = ipc_channel_create(mem, test_utils::SMALL_BUFFER_SIZE, invalid_config);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel create - success case") {
    uint8_t mem[test_utils::SMALL_BUFFER_SIZE];
    const IpcChannelResult result = ipc_channel_create(mem, test_utils::SMALL_BUFFER_SIZE, test_utils::DEFAULT_CONFIG);
    test_utils::CHECK_OK(result);
    
    IpcChannel* channel = result.result;
    test_utils::verify_channel_creation(channel);
    
    ipc_channel_destroy(channel);
}

TEST_CASE("channel create - error structure verification") {
    const IpcChannelResult null_result = ipc_channel_create(nullptr, test_utils::SMALL_BUFFER_SIZE, test_utils::DEFAULT_CONFIG);
    CHECK(IpcChannelResult_is_error(null_result));
    CHECK(null_result.error.body.requested_size == test_utils::SMALL_BUFFER_SIZE);
    CHECK(null_result.error.body.min_size > 0);
    
    
    uint8_t mem[test_utils::SMALL_BUFFER_SIZE];
    IpcChannelConfiguration invalid_config = {0, 1000, 100000};
    const IpcChannelResult config_result = ipc_channel_create(mem, test_utils::SMALL_BUFFER_SIZE, invalid_config);
    CHECK(IpcChannelResult_is_error(config_result));
    CHECK(config_result.error.body.requested_size == test_utils::SMALL_BUFFER_SIZE);
    CHECK(config_result.error.body.min_size > 0);
}


TEST_CASE("channel connect - NULL memory") {
    const IpcChannelConnectResult result = ipc_channel_connect(nullptr, test_utils::DEFAULT_CONFIG);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel connect - invalid config") {
    uint8_t mem[128];
    IpcChannelConfiguration invalid_config = {0, 1000, 100000};
    const IpcChannelConnectResult result = ipc_channel_connect(mem, invalid_config);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel connect - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    const IpcChannelConnectResult result = ipc_channel_connect(
        const_cast<uint8_t*>(channel.get_mem()), test_utils::DEFAULT_CONFIG);
    test_utils::CHECK_OK(result);
    
    IpcChannel* connected_channel = result.result;
    CHECK(connected_channel != nullptr);
    
    ipc_channel_destroy(connected_channel);
}

TEST_CASE("channel connect - error structure verification") {
    
    const IpcChannelConnectResult null_result = ipc_channel_connect(nullptr, test_utils::DEFAULT_CONFIG);
    CHECK(IpcChannelConnectResult_is_error(null_result));
    CHECK(null_result.error.body.min_size > 0);
    
    
    uint8_t mem[128];
    IpcChannelConfiguration invalid_config = {0, 1000, 100000};
    const IpcChannelConnectResult config_result = ipc_channel_connect(mem, invalid_config);
    CHECK(IpcChannelConnectResult_is_error(config_result));
    CHECK(config_result.error.body.min_size > 0);
}


TEST_CASE("channel destroy - NULL channel") {
    const IpcChannelDestroyResult result = ipc_channel_destroy(nullptr);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel destroy - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    // Release ownership from wrapper to avoid double-free in destructor
    IpcChannel* ch = channel.release();
    const IpcChannelDestroyResult result = ipc_channel_destroy(ch);
    test_utils::CHECK_OK(result);
}

TEST_CASE("channel destroy - error structure verification") {
    
    const IpcChannelDestroyResult null_result = ipc_channel_destroy(nullptr);
    CHECK(IpcChannelDestroyResult_is_error(null_result));
    CHECK(null_result.error.body._unit == false);
}


TEST_CASE("channel write - NULL channel") {
    const int test_data = 42;
    const IpcChannelWriteResult result = ipc_channel_write(nullptr, &test_data, sizeof(test_data));
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel write - NULL data") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const IpcChannelWriteResult result = ipc_channel_write(channel.get(), nullptr, sizeof(int));
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel write - zero size") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    const IpcChannelWriteResult result = ipc_channel_write(channel.get(), &test_data, 0);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel write - entry too large") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    std::vector<uint8_t> large_data(1000);
    const IpcChannelWriteResult result = ipc_channel_write(channel.get(), large_data.data(), large_data.size());
    test_utils::CHECK_ERROR(result, IPC_ERR_ENTRY_TOO_LARGE);
}

TEST_CASE("channel write - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    const IpcChannelWriteResult result = ipc_channel_write(channel.get(), &test_data, sizeof(test_data));
    test_utils::CHECK_OK(result);
}

TEST_CASE("channel write - error structure verification") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    const int test_data = 42;
    const IpcChannelWriteResult null_channel_result = ipc_channel_write(nullptr, &test_data, sizeof(test_data));
    CHECK(IpcChannelWriteResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body.requested_size == sizeof(test_data));
    
    
    const IpcChannelWriteResult null_data_result = ipc_channel_write(channel.get(), nullptr, sizeof(test_data));
    CHECK(IpcChannelWriteResult_is_error(null_data_result));
    CHECK(null_data_result.error.body.requested_size == sizeof(test_data));
    
    
    const IpcChannelWriteResult zero_size_result = ipc_channel_write(channel.get(), &test_data, 0);
    CHECK(IpcChannelWriteResult_is_error(zero_size_result));
    CHECK(zero_size_result.error.body.requested_size == 0);
}


TEST_CASE("channel read - NULL channel") {
    IpcEntry entry;
    const IpcChannelReadResult result = ipc_channel_read(nullptr, &entry);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel read - NULL dest") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const IpcChannelReadResult result = ipc_channel_read(channel.get(), nullptr);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel read - empty channel") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    IpcEntry entry;
    const IpcChannelReadResult result = ipc_channel_read(channel.get(), &entry);
    bool is_empty_or_retry_limit = (result.ipc_status == IPC_EMPTY || result.ipc_status == IPC_ERR_RETRY_LIMIT);
    CHECK(is_empty_or_retry_limit);
}

TEST_CASE("channel read - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelReadResult result = ipc_channel_read(channel.get(), &entry);
    test_utils::CHECK_OK(result);
    
    int read_data;
    memcpy(&read_data, entry.payload, sizeof(test_data));
    CHECK(read_data == test_data);
    free(entry.payload);
}

TEST_CASE("channel read - error structure verification") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    IpcEntry entry;
    const IpcChannelReadResult null_channel_result = ipc_channel_read(nullptr, &entry);
    CHECK(IpcChannelReadResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body.offset == 0);
    
    
    const IpcChannelReadResult null_dest_result = ipc_channel_read(channel.get(), nullptr);
    CHECK(IpcChannelReadResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
}


TEST_CASE("channel read_with_timeout - NULL channel") {
    IpcEntry entry;
    struct timespec timeout = {0, 1000000};
    const IpcChannelReadWithTimeoutResult result = ipc_channel_read_with_timeout(nullptr, &entry, &timeout);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel read_with_timeout - NULL dest") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    struct timespec timeout = {0, 1000000};
    const IpcChannelReadWithTimeoutResult result = ipc_channel_read_with_timeout(channel.get(), nullptr, &timeout);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel read_with_timeout - NULL timeout") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    IpcEntry entry;
    const IpcChannelReadWithTimeoutResult result = ipc_channel_read_with_timeout(channel.get(), &entry, nullptr);
    CHECK(IpcChannelReadWithTimeoutResult_is_error(result));
}

TEST_CASE("channel read_with_timeout - timeout case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    struct timespec timeout = {0, 1000000};
    IpcEntry entry;
    const IpcChannelReadWithTimeoutResult result = ipc_channel_read_with_timeout(channel.get(), &entry, &timeout);
    CHECK(result.ipc_status == IPC_ERR_TIMEOUT);
}

TEST_CASE("channel read_with_timeout - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    struct timespec timeout = {1, 0}; 
    IpcEntry entry;
    const IpcChannelReadWithTimeoutResult result = ipc_channel_read_with_timeout(channel.get(), &entry, &timeout);
    test_utils::CHECK_OK(result);
    
    int read_data;
    memcpy(&read_data, entry.payload, sizeof(test_data));
    CHECK(read_data == test_data);
    free(entry.payload);
}

TEST_CASE("channel read_with_timeout - error structure verification") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    IpcEntry entry;
    struct timespec timeout = {0, 1000000};
    const IpcChannelReadWithTimeoutResult null_channel_result = ipc_channel_read_with_timeout(nullptr, &entry, &timeout);
    CHECK(IpcChannelReadWithTimeoutResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body.offset == 0);
    
    
    const IpcChannelReadWithTimeoutResult null_dest_result = ipc_channel_read_with_timeout(channel.get(), nullptr, &timeout);
    CHECK(IpcChannelReadWithTimeoutResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
    
    const IpcChannelReadWithTimeoutResult null_timeout_result = ipc_channel_read_with_timeout(channel.get(), &entry, nullptr);
    CHECK(IpcChannelReadWithTimeoutResult_is_error(null_timeout_result));
}


TEST_CASE("channel try_read - NULL channel") {
    IpcEntry entry;
    const IpcChannelTryReadResult result = ipc_channel_try_read(nullptr, &entry);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel try_read - NULL dest") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const IpcChannelTryReadResult result = ipc_channel_try_read(channel.get(), nullptr);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel try_read - empty channel") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    IpcEntry entry;
    const IpcChannelTryReadResult result = ipc_channel_try_read(channel.get(), &entry);
    CHECK(result.ipc_status == IPC_EMPTY);
}

TEST_CASE("channel try_read - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelTryReadResult result = ipc_channel_try_read(channel.get(), &entry);
    test_utils::CHECK_OK(result);
    
    int read_data;
    memcpy(&read_data, entry.payload, sizeof(test_data));
    CHECK(read_data == test_data);
    free(entry.payload);
}

TEST_CASE("channel try_read - error structure verification") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    IpcEntry entry;
    const IpcChannelTryReadResult null_channel_result = ipc_channel_try_read(nullptr, &entry);
    CHECK(IpcChannelTryReadResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body.offset == 0);
    
    
    const IpcChannelTryReadResult null_dest_result = ipc_channel_try_read(channel.get(), nullptr);
    CHECK(IpcChannelTryReadResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
}


TEST_CASE("channel peek - NULL channel") {
    IpcEntry entry;
    const IpcChannelPeekResult result = ipc_channel_peek(nullptr, &entry);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel peek - NULL dest") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const IpcChannelPeekResult result = ipc_channel_peek(channel.get(), nullptr);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel peek - empty channel") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    IpcEntry entry;
    const IpcChannelPeekResult result = ipc_channel_peek(channel.get(), &entry);
    CHECK(result.ipc_status == IPC_EMPTY);
}

TEST_CASE("channel peek - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelPeekResult result = ipc_channel_peek(channel.get(), &entry);
    test_utils::CHECK_OK(result);
    
    int peeked_data;
    memcpy(&peeked_data, entry.payload, sizeof(test_data));
    CHECK(peeked_data == test_data);
    
    IpcEntry entry2;
    const IpcChannelPeekResult result2 = ipc_channel_peek(channel.get(), &entry2);
    test_utils::CHECK_OK(result2);
    CHECK(entry.offset == entry2.offset);
    CHECK(entry.size == entry2.size);
}

TEST_CASE("channel peek - error structure verification") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    
    IpcEntry entry;
    const IpcChannelPeekResult null_channel_result = ipc_channel_peek(nullptr, &entry);
    CHECK(IpcChannelPeekResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body.offset == 0);
    
    
    const IpcChannelPeekResult null_dest_result = ipc_channel_peek(channel.get(), nullptr);
    CHECK(IpcChannelPeekResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
}


TEST_CASE("channel skip - NULL channel") {
    const IpcChannelSkipResult result = ipc_channel_skip(nullptr, 0);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel skip - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelPeekResult peek_result = ipc_channel_peek(channel.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    
    const IpcChannelSkipResult skip_result = ipc_channel_skip(channel.get(), entry.offset);
    test_utils::CHECK_OK(skip_result);
    CHECK(skip_result.result == entry.offset);
    
    const IpcChannelPeekResult peek_after = ipc_channel_peek(channel.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}

TEST_CASE("channel skip - wrong offset") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    const IpcChannelSkipResult skip_result = ipc_channel_skip(channel.get(), 256);
    test_utils::CHECK_ERROR(skip_result, IPC_ERR_OFFSET_MISMATCH);
}

TEST_CASE("channel skip - empty channel") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    const IpcChannelSkipResult skip_result = ipc_channel_skip(channel.get(), 0);
    CHECK(skip_result.ipc_status == IPC_EMPTY);
}

TEST_CASE("channel skip - error structure verification") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    
    const IpcChannelSkipResult null_channel_result = ipc_channel_skip(nullptr, 0);
    CHECK(IpcChannelSkipResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body.offset == 0);
    
    
    const IpcChannelSkipResult wrong_offset_result = ipc_channel_skip(channel.get(), 256);
    CHECK(IpcChannelSkipResult_is_error(wrong_offset_result));
    CHECK(wrong_offset_result.error.body.offset == 0);
}


TEST_CASE("channel skip_force - NULL channel") {
    const IpcChannelSkipForceResult result = ipc_channel_skip_force(nullptr);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("channel skip_force - success case") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelPeekResult peek_result = ipc_channel_peek(channel.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    
    const IpcChannelSkipForceResult skip_force_result = ipc_channel_skip_force(channel.get());
    test_utils::CHECK_OK(skip_force_result);
    CHECK(skip_force_result.result == 0);
    
    const IpcChannelPeekResult peek_after = ipc_channel_peek(channel.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}

TEST_CASE("channel skip_force - empty channel") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    const IpcChannelSkipForceResult skip_force_result = ipc_channel_skip_force(channel.get());
    CHECK(skip_force_result.ipc_status == IPC_EMPTY);
    CHECK(skip_force_result.result == 0);
}

TEST_CASE("channel skip_force - error structure verification") {
    
    const IpcChannelSkipForceResult null_channel_result = ipc_channel_skip_force(nullptr);
    CHECK(IpcChannelSkipForceResult_is_error(null_channel_result));
    CHECK(null_channel_result.error.body._unit == false);
}


TEST_CASE("channel integration - write read sequence") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    const int read_data = test_utils::read_data<int>(channel.get());
    CHECK(read_data == test_data);
}

TEST_CASE("channel integration - write peek skip sequence") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    IpcEntry entry;
    const IpcChannelPeekResult peek_result = ipc_channel_peek(channel.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    
    int peeked_data;
    memcpy(&peeked_data, entry.payload, sizeof(test_data));
    CHECK(peeked_data == test_data);
    
    const IpcChannelSkipResult skip_result = ipc_channel_skip(channel.get(), entry.offset);
    test_utils::CHECK_OK(skip_result);
    
    const IpcChannelPeekResult peek_after = ipc_channel_peek(channel.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}

TEST_CASE("channel integration - multiple entries") {
    test_utils::ChannelWrapper channel(test_utils::MEDIUM_BUFFER_SIZE);
    
    const int v1 = 1, v2 = 2, v3 = 3;
    test_utils::write_data(channel.get(), v1);
    test_utils::write_data(channel.get(), v2);
    test_utils::write_data(channel.get(), v3);
    
    const int read_v1 = test_utils::read_data<int>(channel.get());
    CHECK(read_v1 == v1);
    
    const int read_v2 = test_utils::read_data<int>(channel.get());
    CHECK(read_v2 == v2);
    
    const int read_v3 = test_utils::read_data<int>(channel.get());
    CHECK(read_v3 == v3);
}

TEST_CASE("channel integration - timeout behavior") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    struct timespec timeout = {0, 1000000};
    IpcEntry entry;
    const IpcChannelReadWithTimeoutResult timeout_result = 
        ipc_channel_read_with_timeout(channel.get(), &entry, &timeout);
    CHECK(timeout_result.ipc_status == IPC_ERR_TIMEOUT);
    
    const int test_data = 42;
    test_utils::write_data(channel.get(), test_data);
    
    struct timespec long_timeout = {1, 0};
    const IpcChannelReadWithTimeoutResult success_result = 
        ipc_channel_read_with_timeout(channel.get(), &entry, &long_timeout);
    test_utils::CHECK_OK(success_result);
    
    int read_data;
    memcpy(&read_data, entry.payload, sizeof(test_data));
    CHECK(read_data == test_data);
    free(entry.payload);
}


TEST_CASE("channel boundary - maximum size data") {
    test_utils::ChannelWrapper channel(test_utils::LARGE_BUFFER_SIZE);
    
    const size_t max_data_size = test_utils::LARGE_BUFFER_SIZE - 100;
    
    std::vector<uint8_t> large_data(max_data_size, 0xAB);
    
    IpcChannelWriteResult write_result = 
        ipc_channel_write(channel.get(), large_data.data(), large_data.size());
    
    if (IpcChannelWriteResult_is_ok(write_result)) {
        IpcEntry entry;
        const IpcChannelReadResult read_result = ipc_channel_read(channel.get(), &entry);
        test_utils::CHECK_OK(read_result);
        
        CHECK(entry.size == max_data_size);
        
        CHECK(memcmp(entry.payload, large_data.data(), max_data_size) == 0);
        free(entry.payload);
    }
}

TEST_CASE("channel boundary - single byte operations") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    uint8_t single_byte = 0xFF;
    test_utils::write_data(channel.get(), single_byte);
    
    const uint8_t read_byte = test_utils::read_data<uint8_t>(channel.get());
    CHECK(read_byte == single_byte);
}

TEST_CASE("channel boundary - exact minimum size") {
    const uint64_t min_size = ipc_channel_align_size(1);
    std::vector<uint8_t> mem(min_size);
    const IpcChannelResult result = ipc_channel_create(mem.data(), min_size, test_utils::DEFAULT_CONFIG);
    test_utils::CHECK_OK(result);
    
    IpcChannel* channel = result.result;
    
    
    ipc_channel_destroy(channel);
}

TEST_CASE("channel boundary - configuration limits") {
    uint8_t mem[128];
    
    IpcChannelConfiguration max_config = {
        .max_round_trips = UINT32_MAX,
        .start_sleep_ns = LONG_MAX,
        .max_sleep_ns = LONG_MAX
    };
    
    const IpcChannelResult result = ipc_channel_create(mem, 128, max_config);
    test_utils::CHECK_OK(result);
    
    IpcChannel* channel = result.result;
    ipc_channel_destroy(channel);
}

TEST_CASE("channel boundary - timeout edge cases") {
    test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
    
    struct timespec tiny_timeout = {0, 1};
    IpcEntry entry;
    const IpcChannelReadWithTimeoutResult result = 
        ipc_channel_read_with_timeout(channel.get(), &entry, &tiny_timeout);
    CHECK(result.ipc_status == IPC_ERR_TIMEOUT);
    
    struct timespec zero_timeout = {0, 0};
    const IpcChannelReadWithTimeoutResult zero_result = 
        ipc_channel_read_with_timeout(channel.get(), &entry, &zero_timeout);
    CHECK(zero_result.ipc_status == IPC_ERR_TIMEOUT);
}
