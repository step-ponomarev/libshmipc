#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "shmipc/ipc_common.h"
#include "shmipc/ipc_buffer.h"
#include <cstring>

TEST_CASE("create too small buffer") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 0);
    test_utils::CHECK_ERROR(buffer_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("size align function") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result =
        ipc_buffer_create(mem, ipc_buffer_align_size(0));
    test_utils::CHECK_OK(buffer_result);
}

TEST_CASE("create buffer with NULL memory") {
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(nullptr, 128);
    test_utils::CHECK_ERROR_WITH_FIELDS(buffer_result, IPC_ERR_INVALID_ARGUMENT, 128, 26);
}

TEST_CASE("create buffer too small size with error fields") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 10);
    test_utils::CHECK_ERROR_WITH_FIELDS(buffer_result, IPC_ERR_INVALID_ARGUMENT, 10, 26);
}

TEST_CASE("create buffer with minimum valid size") {
    uint8_t mem[128];
    // MIN_BUFFER_SIZE = sizeof(IpcBufferHeader) + 2 = 24 + 2 = 26
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 26);
    test_utils::CHECK_OK(buffer_result);
}

TEST_CASE("create buffer with different valid sizes") {
    uint8_t mem[1024];
    
    // Test various valid sizes
    const size_t test_sizes[] = {26, 32, 64, 128, 256, 512, 1024};
    
    for (size_t size : test_sizes) {
        const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, size);
        test_utils::CHECK_OK(buffer_result);
        free(buffer_result.result); // Clean up
    }
}

TEST_CASE("create buffer success case verification") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 128);
    
    test_utils::CHECK_OK(buffer_result);
    
    // Verify that the buffer was actually created and can be used
    IpcBuffer* buffer = buffer_result.result;
    CHECK(buffer != nullptr);
    
    // Try to write something to verify it works
    const int test_value = 42;
    const IpcBufferWriteResult write_result = ipc_buffer_write(buffer, &test_value, sizeof(test_value));
    CHECK(write_result.ipc_status == IPC_OK);
    
    // Clean up
    free(buffer);
}

// Note: Testing malloc failure is difficult without mocking, but we can at least
// verify that the error handling code path exists by checking the error structure
TEST_CASE("create buffer error structure verification") {
    uint8_t mem[128];
    
    // Test NULL pointer case
    const IpcBufferCreateResult null_result = ipc_buffer_create(nullptr, 128);
    CHECK(IpcBufferCreateResult_is_error(null_result));
    CHECK(null_result.error.body.requested_size == 128);
    CHECK(null_result.error.body.min_size == 26); // sizeof(IpcBufferHeader) + 2
    
    // Test too small size case
    const IpcBufferCreateResult small_result = ipc_buffer_create(mem, 5);
    CHECK(IpcBufferCreateResult_is_error(small_result));
    CHECK(small_result.error.body.requested_size == 5);
    CHECK(small_result.error.body.min_size == 26);
}

// ==================== Tests for ipc_buffer_attach ====================

TEST_CASE("attach buffer with NULL memory") {
    const IpcBufferAttachResult attach_result = ipc_buffer_attach(nullptr);
    test_utils::CHECK_ERROR_WITH_FIELDS(attach_result, IPC_ERR_INVALID_ARGUMENT, 26);
}

TEST_CASE("attach buffer success case") {
    uint8_t mem[128];
    
    // First create a buffer
    const IpcBufferCreateResult create_result = ipc_buffer_create(mem, 128);
    test_utils::CHECK_OK(create_result);
    IpcBuffer* created_buffer = create_result.result;
    
    // Write some data to the buffer
    const int test_value = 42;
    const IpcBufferWriteResult write_result = ipc_buffer_write(created_buffer, &test_value, sizeof(test_value));
    CHECK(write_result.ipc_status == IPC_OK);
    
    // Now attach to the same memory
    const IpcBufferAttachResult attach_result = ipc_buffer_attach(mem);
    test_utils::CHECK_OK(attach_result);
    IpcBuffer* attached_buffer = attach_result.result;
    
    // Verify that we can read the same data
    test_utils::EntryWrapper entry(sizeof(test_value));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result = ipc_buffer_read(attached_buffer, &entry_ref);
    CHECK(read_result.ipc_status == IPC_OK);
    
    int read_value;
    memcpy(&read_value, entry_ref.payload, sizeof(test_value));
    CHECK(read_value == test_value);
    
    // Clean up
    free(created_buffer);
    free(attached_buffer);
}

TEST_CASE("attach buffer error structure verification") {
    // Test NULL pointer case
    const IpcBufferAttachResult null_result = ipc_buffer_attach(nullptr);
    CHECK(IpcBufferAttachResult_is_error(null_result));
    CHECK(null_result.error.body.min_size == 26); // sizeof(IpcBufferHeader) + 2
}

TEST_CASE("single entry") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    const int eval = 12;
    test_utils::write_data(buffer.get(), eval);
    
    const int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == eval);
}

TEST_CASE("fill buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t added_count = 0;
    while (IpcBufferWriteResult_is_ok(
               ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t))) &&
           (++added_count))
        ;

    const IpcBufferWriteResult status_result =
        ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t));
    test_utils::CHECK_ERROR(status_result, IPC_ERR_NO_SPACE_CONTIGUOUS);

    test_utils::EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < added_count; i++) {
        IpcEntry entry_ref = entry.get();
        const IpcBufferReadResult read_res = ipc_buffer_read(buffer.get(), &entry_ref);
        test_utils::CHECK_OK(read_res);
        CHECK(read_res.ipc_status == IPC_OK);
        CHECK(entry_ref.size == sizeof(size_t));

        size_t res;
        memcpy(&res, entry_ref.payload, entry_ref.size);
        CHECK(res == i);
    }

    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_res = ipc_buffer_read(buffer.get(), &entry_ref);
    test_utils::CHECK_OK(read_res);
    CHECK(read_res.ipc_status == IPC_EMPTY);
}

TEST_CASE("add to full buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t added_count = 0;
    while (IpcBufferWriteResult_is_ok(
               ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t))) &&
           (++added_count))
        ;

    CHECK(ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t)).ipc_status ==
          IPC_ERR_NO_SPACE_CONTIGUOUS);
}

TEST_CASE("wrap buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t added_count = 0;
    while (ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t)).ipc_status ==
               IPC_OK &&
           (++added_count))
        ;

    CHECK(ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t)).ipc_status ==
          IPC_ERR_NO_SPACE_CONTIGUOUS);

    CHECK(ipc_buffer_skip_force(buffer.get()).ipc_status == IPC_OK);

    const size_t last_val = 666;
    CHECK(ipc_buffer_write(buffer.get(), &last_val, sizeof(last_val)).ipc_status ==
          IPC_OK);

    test_utils::EntryWrapper entry(sizeof(size_t));
    size_t prev;

    while (true) {
        IpcEntry entry_ref = entry.get();
        if (ipc_buffer_read(buffer.get(), &entry_ref).ipc_status != IPC_OK) {
            break;
        }
        CHECK(entry_ref.size == sizeof(size_t));
        memcpy(&prev, entry_ref.payload, entry_ref.size);
    }

    CHECK(prev == last_val);
}

TEST_CASE("peek") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    test_utils::write_data(buffer.get(), expected_val);

    const int peeked_val = test_utils::peek_data<int>(buffer.get());
    CHECK(expected_val == peeked_val);

    test_utils::EntryWrapper entry(sizeof(expected_val));
    IpcEntry entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    
    int val;
    memcpy(&val, entry_ref.payload, sizeof(expected_val));
    CHECK(expected_val == val);

    IpcEntry empty_entry;
    CHECK(ipc_buffer_peek(buffer.get(), &empty_entry).ipc_status == IPC_EMPTY);
}

TEST_CASE("skip") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    test_utils::write_data(buffer.get(), expected_val);

    IpcEntry entry;
    const IpcBufferPeekResult peek_res = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_res);
    CHECK(ipc_buffer_skip(buffer.get(), entry.offset).ipc_status == IPC_OK);
    CHECK(ipc_buffer_peek(buffer.get(), &entry).ipc_status == IPC_EMPTY);
}

TEST_CASE("double skip") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    test_utils::write_data(buffer.get(), expected_val);

    IpcEntry entry;
    const IpcBufferPeekResult peek_res = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_res);
    CHECK(ipc_buffer_skip(buffer.get(), entry.offset).ipc_status == IPC_OK);

    const IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer.get(), entry.offset);
    CHECK(IpcBufferSkipResult_is_error(skip_result));
    CHECK(skip_result.ipc_status == IPC_ERR_OFFSET_MISMATCH);
    CHECK(ipc_buffer_peek(buffer.get(), &entry).ipc_status == IPC_EMPTY);
}

TEST_CASE("skip forced") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    test_utils::write_data(buffer.get(), expected_val);

    IpcEntry entry;
    CHECK(ipc_buffer_peek(buffer.get(), &entry).ipc_status == IPC_OK);
    CHECK(ipc_buffer_skip_force(buffer.get()).ipc_status == IPC_OK);
    CHECK(ipc_buffer_peek(buffer.get(), &entry).ipc_status == IPC_EMPTY);
}

TEST_CASE("skip with incorrect id") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    test_utils::write_data(buffer.get(), expected_val);

    IpcEntry entry;
    const IpcBufferPeekResult result = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(result);

    IpcEntry entry2;
    const IpcBufferPeekResult result2 = ipc_buffer_peek(buffer.get(), &entry2);
    test_utils::CHECK_OK(result2);
    CHECK(entry.offset == entry2.offset);
    CHECK(entry.size == entry2.size);

    int val1;
    memcpy(&val1, entry.payload, entry.size);

    int val2;
    memcpy(&val2, entry2.payload, entry2.size);

    CHECK(val1 == val2);
}

TEST_CASE("peek consistency") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

    int v1 = 1, v2 = 2;
    test_utils::write_data(buffer.get(), v1);
    test_utils::write_data(buffer.get(), v2);

    IpcEntry entry;
    test_utils::CHECK_OK(ipc_buffer_peek(buffer.get(), &entry));

    int seen;
    memcpy(&seen, entry.payload, sizeof(seen));
    CHECK(seen == v1);

    CHECK(ipc_buffer_skip_force(buffer.get()).ipc_status == IPC_OK);

    test_utils::CHECK_OK(ipc_buffer_peek(buffer.get(), &entry));
    memcpy(&seen, entry.payload, sizeof(seen));
    CHECK(seen == v2);
}

TEST_CASE("read too small") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    int val = 42;
    test_utils::write_data(buffer.get(), val);

    test_utils::EntryWrapper entry(sizeof(val) - 1);
    IpcEntry entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref).ipc_status == IPC_ERR_TOO_SMALL);
}

TEST_CASE("reserve commit read") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    int *data;

    const IpcBufferReserveEntryResult result =
        ipc_buffer_reserve_entry(buffer.get(), sizeof(expected_val), reinterpret_cast<void **>(&data));
    CHECK(result.ipc_status == IPC_OK);

    test_utils::EntryWrapper entry(sizeof(expected_val));
    IpcEntry entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref).ipc_status == IPC_ERR_NOT_READY);

    *data = expected_val;
    CHECK(ipc_buffer_commit_entry(buffer.get(), result.result).ipc_status == IPC_OK);
    
    entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    CHECK(entry_ref.size == sizeof(expected_val));

    int res;
    memcpy(&res, entry_ref.payload, entry_ref.size);
    CHECK(res == expected_val);
}

TEST_CASE("reserve double commit") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int expected_val = 12;
    int *data;

    const IpcBufferReserveEntryResult result =
        ipc_buffer_reserve_entry(buffer.get(), sizeof(expected_val), reinterpret_cast<void **>(&data));
    CHECK(result.ipc_status == IPC_OK);

    test_utils::EntryWrapper entry(sizeof(expected_val));
    IpcEntry entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref).ipc_status == IPC_ERR_NOT_READY);

    *data = expected_val;
    CHECK(ipc_buffer_commit_entry(buffer.get(), result.result).ipc_status == IPC_OK);
    CHECK(IpcBufferCommitEntryResult_is_error(
        ipc_buffer_commit_entry(buffer.get(), result.result)));
    
    entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    CHECK(entry_ref.size == sizeof(expected_val));

    int res;
    memcpy(&res, entry_ref.payload, entry_ref.size);
    CHECK(res == expected_val);
}

TEST_CASE("multiple reserve commit read") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);

    for (int i = 0; i < 10; ++i) {
        int *ptr;
        const IpcBufferReserveEntryResult result =
            ipc_buffer_reserve_entry(buffer.get(), sizeof(int), reinterpret_cast<void **>(&ptr));

        CHECK(result.ipc_status == IPC_OK);
        *ptr = i;
        CHECK(ipc_buffer_commit_entry(buffer.get(), result.result).ipc_status == IPC_OK);
    }

    test_utils::EntryWrapper entry(sizeof(int));
    for (int i = 0; i < 10; ++i) {
        IpcEntry entry_ref = entry.get();
        test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
        
        int val;
        memcpy(&val, entry_ref.payload, sizeof(int));
        CHECK(val == i);
    }
}
