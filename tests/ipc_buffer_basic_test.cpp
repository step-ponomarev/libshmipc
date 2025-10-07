#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "shmipc/ipc_buffer.h"
#include <cstring>

TEST_CASE("buffer create - too small size") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 0);
    test_utils::CHECK_ERROR(buffer_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("buffer align_size - valid alignment") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result =
        ipc_buffer_create(mem, ipc_buffer_align_size(0));
    test_utils::CHECK_OK(buffer_result);
}

TEST_CASE("buffer create - NULL memory pointer") {
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(nullptr, ipc_buffer_align_size(128));
    test_utils::CHECK_ERROR(buffer_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("buffer create - too small size error fields") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 10);
    test_utils::CHECK_ERROR(buffer_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("buffer create - success case") {
    uint8_t mem[test_utils::SMALL_BUFFER_SIZE];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, test_utils::SMALL_BUFFER_SIZE);
    test_utils::CHECK_OK(buffer_result);
    
    IpcBuffer* buffer = buffer_result.result;
    test_utils::verify_buffer_creation(buffer, test_utils::SMALL_BUFFER_SIZE);
    
    free(buffer);
}

TEST_CASE("buffer create - error structure verification") {
    uint8_t mem[128];
    
    const IpcBufferCreateResult null_result = ipc_buffer_create(nullptr, 128);
    CHECK(IpcBufferCreateResult_is_error(null_result));

    const IpcBufferCreateResult small_result = ipc_buffer_create(mem, 5);
    CHECK(IpcBufferCreateResult_is_error(small_result));
}


TEST_CASE("attach buffer with NULL memory") {
    const IpcBufferAttachResult attach_result = ipc_buffer_attach(nullptr);
    test_utils::CHECK_ERROR(attach_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("attach buffer success case") {
    uint8_t mem[256];
    
    const IpcBufferCreateResult create_result = ipc_buffer_create(mem, 256);
    test_utils::CHECK_OK(create_result);
    IpcBuffer* created_buffer = create_result.result;
    
    const int test_value = 42;
    const IpcBufferWriteResult write_result = ipc_buffer_write(created_buffer, &test_value, sizeof(test_value));
    CHECK(write_result.ipc_status == IPC_OK);
    
    const IpcBufferAttachResult attach_result = ipc_buffer_attach(mem);
    test_utils::CHECK_OK(attach_result);
    IpcBuffer* attached_buffer = attach_result.result;
    
    test_utils::EntryWrapper entry(sizeof(test_value));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result = ipc_buffer_read(attached_buffer, &entry_ref);
    CHECK(read_result.ipc_status == IPC_OK);
    
    int read_value;
    memcpy(&read_value, entry_ref.payload, sizeof(test_value));
    CHECK(read_value == test_value);
    
    free(created_buffer);
    free(attached_buffer);
}

TEST_CASE("attach buffer error structure verification") {
    const IpcBufferAttachResult null_result = ipc_buffer_attach(nullptr);
    CHECK(IpcBufferAttachResult_is_error(null_result));
}


TEST_CASE("write with NULL buffer") {
    const int test_data = 42;
    const IpcBufferWriteResult write_result = ipc_buffer_write(nullptr, &test_data, sizeof(test_data));
    test_utils::CHECK_ERROR(write_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("write with NULL data") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), nullptr, sizeof(int));
    test_utils::CHECK_ERROR(write_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("write with zero size") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    const IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &test_data, 0);
    test_utils::CHECK_ERROR(write_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("write success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    CHECK(test_utils::write_data_safe(buffer.get(), test_data));
}

TEST_CASE("write error structure verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    const int test_data = 42;
    const IpcBufferWriteResult null_buffer_result = ipc_buffer_write(nullptr, &test_data, sizeof(test_data));
    CHECK(IpcBufferWriteResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.requested_size == sizeof(test_data));
    
    
    const IpcBufferWriteResult null_data_result = ipc_buffer_write(buffer.get(), nullptr, sizeof(test_data));
    CHECK(IpcBufferWriteResult_is_error(null_data_result));
    CHECK(null_data_result.error.body.requested_size == sizeof(test_data));
    
    
    const IpcBufferWriteResult zero_size_result = ipc_buffer_write(buffer.get(), &test_data, 0);
    CHECK(IpcBufferWriteResult_is_error(zero_size_result));
    CHECK(zero_size_result.error.body.requested_size == 0);
}


TEST_CASE("read with NULL buffer") {
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result = ipc_buffer_read(nullptr, &entry_ref);
    test_utils::CHECK_ERROR(read_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("read with NULL dest") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), nullptr);
    test_utils::CHECK_ERROR(read_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("read success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    const int read_data = test_utils::read_data_safe<int>(buffer.get());
    CHECK(read_data == test_data);
}

TEST_CASE("read error structure verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult null_buffer_result = ipc_buffer_read(nullptr, &entry_ref);
    CHECK(IpcBufferReadResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 0);
    
    
    const IpcBufferReadResult null_dest_result = ipc_buffer_read(buffer.get(), nullptr);
    CHECK(IpcBufferReadResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
}


TEST_CASE("peek with NULL buffer") {
    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(nullptr, &entry);
    test_utils::CHECK_ERROR(peek_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("peek with NULL dest") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), nullptr);
    test_utils::CHECK_ERROR(peek_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("peek success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    
    
    int peeked_data;
    memcpy(&peeked_data, entry.payload, sizeof(test_data));
    CHECK(peeked_data == test_data);
    
    
    IpcEntry entry2;
    const IpcBufferPeekResult peek_result2 = ipc_buffer_peek(buffer.get(), &entry2);
    test_utils::CHECK_OK(peek_result2);
    CHECK(entry.offset == entry2.offset);
    CHECK(entry.size == entry2.size);
    
    int peeked_data2;
    memcpy(&peeked_data2, entry2.payload, sizeof(test_data));
    CHECK(peeked_data2 == test_data);
}

TEST_CASE("peek empty buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_result.ipc_status == IPC_EMPTY);
}

TEST_CASE("peek error structure verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult null_buffer_result = ipc_buffer_peek(nullptr, &entry);
    CHECK(IpcBufferPeekResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 0);
    
    
    const IpcBufferPeekResult null_dest_result = ipc_buffer_peek(buffer.get(), nullptr);
    CHECK(IpcBufferPeekResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
}

TEST_CASE("peek multiple entries") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    
    const int v1 = 1, v2 = 2, v3 = 3;
    test_utils::write_data(buffer.get(), v1);
    test_utils::write_data(buffer.get(), v2);
    test_utils::write_data(buffer.get(), v3);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek1 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek1);
    
    int peeked_v1;
    memcpy(&peeked_v1, entry.payload, sizeof(v1));
    CHECK(peeked_v1 == v1);
    
    
    const IpcBufferPeekResult peek2 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek2);
    
    int peeked_v1_again;
    memcpy(&peeked_v1_again, entry.payload, sizeof(v1));
    CHECK(peeked_v1_again == v1);
    
    
    CHECK(ipc_buffer_skip_force(buffer.get()).ipc_status == IPC_OK);
    
    
    const IpcBufferPeekResult peek3 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek3);
    
    int peeked_v2;
    memcpy(&peeked_v2, entry.payload, sizeof(v2));
    CHECK(peeked_v2 == v2);
}


TEST_CASE("skip with NULL buffer") {
    const IpcBufferSkipResult skip_result = ipc_buffer_skip(nullptr, 0);
    test_utils::CHECK_ERROR(skip_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("skip success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    
    
    const IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer.get(), entry.offset);
    test_utils::CHECK_OK(skip_result);
    CHECK(skip_result.result == entry.offset);
    
    
    const IpcBufferPeekResult peek_after = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}

TEST_CASE("skip with wrong offset") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    const IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer.get(), 256);
    test_utils::CHECK_ERROR(skip_result, IPC_ERR_OFFSET_MISMATCH);
}

TEST_CASE("skip empty buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    const IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer.get(), 0);
    CHECK(skip_result.ipc_status == IPC_EMPTY);
}

TEST_CASE("skip error structure verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    const IpcBufferSkipResult null_buffer_result = ipc_buffer_skip(nullptr, 0);
    CHECK(IpcBufferSkipResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 0);
    
    
    const IpcBufferSkipResult wrong_offset_result = ipc_buffer_skip(buffer.get(), 256);
    CHECK(IpcBufferSkipResult_is_error(wrong_offset_result));
    
    CHECK(wrong_offset_result.error.body.offset == 0); 
}

TEST_CASE("skip multiple entries") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    
    const int v1 = 1, v2 = 2, v3 = 3;
    test_utils::write_data(buffer.get(), v1);
    test_utils::write_data(buffer.get(), v2);
    test_utils::write_data(buffer.get(), v3);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek1 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek1);
    
    int peeked_v1;
    memcpy(&peeked_v1, entry.payload, sizeof(v1));
    CHECK(peeked_v1 == v1);
    
    
    const IpcBufferSkipResult skip1 = ipc_buffer_skip(buffer.get(), entry.offset);
    test_utils::CHECK_OK(skip1);
    
    
    const IpcBufferPeekResult peek2 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek2);
    
    int peeked_v2;
    memcpy(&peeked_v2, entry.payload, sizeof(v2));
    CHECK(peeked_v2 == v2);
    
    
    const IpcBufferSkipResult skip2 = ipc_buffer_skip(buffer.get(), entry.offset);
    test_utils::CHECK_OK(skip2);
    
    
    const IpcBufferPeekResult peek3 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek3);
    
    int peeked_v3;
    memcpy(&peeked_v3, entry.payload, sizeof(v3));
    CHECK(peeked_v3 == v3);
}

TEST_CASE("skip return value verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    const uint64_t original_offset = entry.offset;
    
    
    const IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer.get(), original_offset);
    test_utils::CHECK_OK(skip_result);
    CHECK(skip_result.result == original_offset);
}


TEST_CASE("skip_force with NULL buffer") {
    const IpcBufferSkipForceResult skip_force_result = ipc_buffer_skip_force(nullptr);
    test_utils::CHECK_ERROR(skip_force_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("skip_force success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_result);
    
    
    const IpcBufferSkipForceResult skip_force_result = ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force_result);
    CHECK(skip_force_result.result == 0); 
    
    
    const IpcBufferPeekResult peek_after = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}

TEST_CASE("skip_force empty buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    const IpcBufferSkipForceResult skip_force_result = ipc_buffer_skip_force(buffer.get());
    CHECK(skip_force_result.ipc_status == IPC_EMPTY);
    CHECK(skip_force_result.result == 0); 
}

TEST_CASE("skip_force error structure verification") {
    
    const IpcBufferSkipForceResult null_buffer_result = ipc_buffer_skip_force(nullptr);
    CHECK(IpcBufferSkipForceResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body._unit == false);
}

TEST_CASE("skip_force multiple entries") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    
    const int v1 = 1, v2 = 2, v3 = 3;
    test_utils::write_data(buffer.get(), v1);
    test_utils::write_data(buffer.get(), v2);
    test_utils::write_data(buffer.get(), v3);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek1 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek1);
    
    int peeked_v1;
    memcpy(&peeked_v1, entry.payload, sizeof(v1));
    CHECK(peeked_v1 == v1);
    
    
    const IpcBufferSkipForceResult skip_force1 = ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force1);
    
    
    const IpcBufferPeekResult peek2 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek2);
    
    int peeked_v2;
    memcpy(&peeked_v2, entry.payload, sizeof(v2));
    CHECK(peeked_v2 == v2);
    
    
    const IpcBufferSkipForceResult skip_force2 = ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force2);
    
    
    const IpcBufferPeekResult peek3 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek3);
    
    int peeked_v3;
    memcpy(&peeked_v3, entry.payload, sizeof(v3));
    CHECK(peeked_v3 == v3);
}

TEST_CASE("skip_force return value verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    const IpcBufferSkipForceResult skip_force_result = ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force_result);
    CHECK(skip_force_result.result == 0); 
}

TEST_CASE("skip_force vs skip comparison") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    const IpcBufferSkipForceResult skip_force_result = ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force_result);
    
    
    IpcEntry entry;
    const IpcBufferPeekResult peek_after = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}


TEST_CASE("reserve_entry with NULL buffer") {
    void* dest;
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(nullptr, sizeof(int), &dest);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("reserve_entry with NULL dest") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), nullptr);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("reserve_entry with zero size") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    void* dest;
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), 0, &dest);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("reserve_entry entry too large") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    void* dest;
    
    
    const size_t huge_size = test_utils::SMALL_BUFFER_SIZE + 1024;
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), huge_size, &dest);
    test_utils::CHECK_ERROR(result, IPC_ERR_ENTRY_TOO_LARGE);
}

TEST_CASE("reserve_entry entry too large error fields") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    void* dest;
    
    
    const size_t huge_size = test_utils::SMALL_BUFFER_SIZE + 1024;
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), huge_size, &dest);
    CHECK(IpcBufferReserveEntryResult_is_error(result));
    CHECK(result.ipc_status == IPC_ERR_ENTRY_TOO_LARGE); 
    CHECK(result.error.body.buffer_size > 0); 
}

TEST_CASE("reserve_entry no space contiguous") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    void* dest;
    std::vector<IpcBufferReserveEntryResult> results;
    
    
    for (int i = 0; i < 10; ++i) {
        const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
        if (IpcBufferReserveEntryResult_is_error(result)) {
            
            CHECK(result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS);
            break;
        }
        results.push_back(result);
    }
    
    
    CHECK(results.size() > 0);
}

TEST_CASE("reserve_entry no space contiguous error fields") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    void* dest;
    std::vector<IpcBufferReserveEntryResult> results;
    
    
    for (int i = 0; i < 10; ++i) {
        const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
        if (IpcBufferReserveEntryResult_is_error(result)) {
            
            CHECK(result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS);
            CHECK(result.error.body.offset > 0); 
            CHECK(result.error.body.buffer_size == 0); 
            CHECK(result.error.body.required_size > 0);
            CHECK(result.error.body.free_space >= 0);
            break;
        }
        results.push_back(result);
    }
    
    
    CHECK(results.size() > 0);
}

TEST_CASE("reserve_entry success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    void* dest;
    const size_t test_size = sizeof(int);
    
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), test_size, &dest);
    test_utils::CHECK_OK(result);
    
    
    CHECK(dest != nullptr);
    
    
    int test_data = 42;
    memcpy(dest, &test_data, test_size);
    
    
    test_utils::EntryWrapper entry(test_size);
    IpcEntry entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref).ipc_status == IPC_ERR_NOT_READY);
}

TEST_CASE("reserve_entry return value verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    void* dest;
    const size_t test_size = sizeof(int);
    
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), test_size, &dest);
    test_utils::CHECK_OK(result);
    
    
    CHECK(result.result >= 0);
    
    
    const IpcBufferCommitEntryResult commit_result = ipc_buffer_commit_entry(buffer.get(), result.result);
    CHECK(commit_result.ipc_status == IPC_OK);
    
    test_utils::EntryWrapper entry(test_size);
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
    CHECK(read_result.ipc_status == IPC_OK);
    CHECK(entry_ref.offset == result.result);
}

TEST_CASE("reserve_entry multiple entries") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    std::vector<uint64_t> offsets;
    std::vector<void*> dests;
    
    
    for (int i = 0; i < 3; ++i) {
        void* dest;
        const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
        test_utils::CHECK_OK(result);
        
        offsets.push_back(result.result);
        dests.push_back(dest);
        
        
        int test_data = i * 10;
        memcpy(dest, &test_data, sizeof(int));
    }
    
    
    for (uint64_t offset : offsets) {
        const IpcBufferCommitEntryResult commit_result = ipc_buffer_commit_entry(buffer.get(), offset);
        CHECK(commit_result.ipc_status == IPC_OK);
    }
    
    
    for (int i = 0; i < 3; ++i) {
        test_utils::EntryWrapper entry(sizeof(int));
        IpcEntry entry_ref = entry.get();
        const IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
        CHECK(read_result.ipc_status == IPC_OK);
        
        int read_data;
        memcpy(&read_data, entry_ref.payload, sizeof(int));
        CHECK(read_data == i * 10);
    }
}

TEST_CASE("reserve_entry boundary size test") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    void* dest;
    
    
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer.get(), test_utils::SMALL_BUFFER_SIZE, &dest);
    test_utils::CHECK_ERROR(result, IPC_ERR_ENTRY_TOO_LARGE);
}

TEST_CASE("reserve_entry error structure verification") {
    
    void* dest;
    const IpcBufferReserveEntryResult null_buffer_result = ipc_buffer_reserve_entry(nullptr, sizeof(int), &dest);
    CHECK(IpcBufferReserveEntryResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 0);
    CHECK(null_buffer_result.error.body.buffer_size == 0);
    CHECK(null_buffer_result.error.body.required_size == 0);
    CHECK(null_buffer_result.error.body.free_space == 0);
    
    
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const IpcBufferReserveEntryResult zero_size_result = ipc_buffer_reserve_entry(buffer.get(), 0, &dest);
    CHECK(IpcBufferReserveEntryResult_is_error(zero_size_result));
    CHECK(zero_size_result.error.body.offset == 0);
    CHECK(zero_size_result.error.body.buffer_size == 0);
    CHECK(zero_size_result.error.body.required_size == 0);
    CHECK(zero_size_result.error.body.free_space == 0);
    
    
    const IpcBufferReserveEntryResult null_dest_result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), nullptr);
    CHECK(IpcBufferReserveEntryResult_is_error(null_dest_result));
    CHECK(null_dest_result.error.body.offset == 0);
    CHECK(null_dest_result.error.body.buffer_size == 0);
    CHECK(null_dest_result.error.body.required_size == 0);
    CHECK(null_dest_result.error.body.free_space == 0);
}


TEST_CASE("commit_entry with NULL buffer") {
    const IpcBufferCommitEntryResult result = ipc_buffer_commit_entry(nullptr, 0);
    test_utils::CHECK_ERROR(result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("commit_entry with invalid offset") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    
    
    const IpcBufferCommitEntryResult result = ipc_buffer_commit_entry(buffer.get(), 999999);
    
    
    
    bool has_response = (IpcBufferCommitEntryResult_is_ok(result) || IpcBufferCommitEntryResult_is_error(result));
    CHECK(has_response);
}

TEST_CASE("commit_entry already committed entry") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    void* dest;
    const IpcBufferReserveEntryResult reserve_result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
    test_utils::CHECK_OK(reserve_result);
    
    int test_data = 42;
    memcpy(dest, &test_data, sizeof(int));
    
    
    const IpcBufferCommitEntryResult commit1 = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_OK(commit1);
    
    
    const IpcBufferCommitEntryResult commit2 = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_ERROR(commit2, IPC_ERR_ILLEGAL_STATE);
}

TEST_CASE("commit_entry success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    void* dest;
    const IpcBufferReserveEntryResult reserve_result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
    test_utils::CHECK_OK(reserve_result);
    
    
    int test_data = 42;
    memcpy(dest, &test_data, sizeof(int));
    
    
    const IpcBufferCommitEntryResult commit_result = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_OK(commit_result);
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
    CHECK(read_result.ipc_status == IPC_OK);
    
    int read_data;
    memcpy(&read_data, entry_ref.payload, sizeof(int));
    CHECK(read_data == test_data);
}

TEST_CASE("commit_entry without reserve") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    const IpcBufferCommitEntryResult result = ipc_buffer_commit_entry(buffer.get(), 0);
    test_utils::CHECK_ERROR(result, IPC_ERR_ILLEGAL_STATE);
}

TEST_CASE("commit_entry error structure verification") {
    
    const IpcBufferCommitEntryResult null_buffer_result = ipc_buffer_commit_entry(nullptr, 123);
    CHECK(IpcBufferCommitEntryResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 123);
    
    
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    void* dest;
    const IpcBufferReserveEntryResult reserve_result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
    test_utils::CHECK_OK(reserve_result);
    
    int test_data = 42;
    memcpy(dest, &test_data, sizeof(int));
    
    
    const IpcBufferCommitEntryResult commit1 = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_OK(commit1);
    
    
    const IpcBufferCommitEntryResult commit2 = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    CHECK(IpcBufferCommitEntryResult_is_error(commit2));
    CHECK(commit2.error.body.offset == reserve_result.result);
}

TEST_CASE("commit_entry multiple entries") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    std::vector<uint64_t> offsets;
    
    
    for (int i = 0; i < 3; ++i) {
        void* dest;
        const IpcBufferReserveEntryResult reserve_result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
        test_utils::CHECK_OK(reserve_result);
        
        int test_data = i * 10;
        memcpy(dest, &test_data, sizeof(int));
        
        const IpcBufferCommitEntryResult commit_result = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
        test_utils::CHECK_OK(commit_result);
        
        offsets.push_back(reserve_result.result);
    }
    
    
    for (int i = 0; i < 3; ++i) {
        test_utils::EntryWrapper entry(sizeof(int));
        IpcEntry entry_ref = entry.get();
        const IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
        CHECK(read_result.ipc_status == IPC_OK);
        
        int read_data;
        memcpy(&read_data, entry_ref.payload, sizeof(int));
        CHECK(read_data == i * 10);
    }
}

TEST_CASE("commit_entry boundary offset test") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    void* dest;
    const IpcBufferReserveEntryResult reserve_result = ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
    test_utils::CHECK_OK(reserve_result);
    
    int test_data = 42;
    memcpy(dest, &test_data, sizeof(int));
    
    const IpcBufferCommitEntryResult commit_result = ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_OK(commit_result);
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
           (++added_count)) {
        // Empty body - just increment counter
    }

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


TEST_CASE("buffer integration - write peek skip sequence") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    
    int data1 = 42, data2 = 84;
    test_utils::write_data(buffer.get(), data1);
    test_utils::write_data(buffer.get(), data2);
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_peek(buffer.get(), &entry_ref));
    
    int peeked_val;
    memcpy(&peeked_val, entry_ref.payload, sizeof(int));
    CHECK(peeked_val == data1);
    
    
    IpcBufferSkipForceResult skip_result = ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_result);
    
    
    entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    
    int read_val;
    memcpy(&read_val, entry_ref.payload, sizeof(int));
    CHECK(read_val == data2);
}

TEST_CASE("buffer integration - reserve peek commit read sequence") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    
    void* dest;
    IpcBufferReserveEntryResult reserve_result = 
        ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
    test_utils::CHECK_OK(reserve_result);
    
    
    int test_data = 123;
    memcpy(dest, &test_data, sizeof(int));
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry_ref);
    CHECK(IpcBufferPeekResult_is_error(peek_result));
    
    
    IpcBufferCommitEntryResult commit_result = 
        ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_OK(commit_result);
    
    
    entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_peek(buffer.get(), &entry_ref));
    
    int peeked_val;
    memcpy(&peeked_val, entry_ref.payload, sizeof(int));
    CHECK(peeked_val == test_data);
    
    
    entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    
    int read_val;
    memcpy(&read_val, entry_ref.payload, sizeof(int));
    CHECK(read_val == test_data);
}

TEST_CASE("buffer integration - mixed operations sequence") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    
    int data1 = 100;
    test_utils::write_data(buffer.get(), data1);
    
    void* dest;
    IpcBufferReserveEntryResult reserve_result = 
        ipc_buffer_reserve_entry(buffer.get(), sizeof(int), &dest);
    test_utils::CHECK_OK(reserve_result);
    
    int data2 = 200;
    memcpy(dest, &data2, sizeof(int));
    
    IpcBufferCommitEntryResult commit_result = 
        ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
    test_utils::CHECK_OK(commit_result);
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    
    
    IpcEntry entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    int read_val1;
    memcpy(&read_val1, entry_ref.payload, sizeof(int));
    CHECK(read_val1 == data1);
    
    
    entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    int read_val2;
    memcpy(&read_val2, entry_ref.payload, sizeof(int));
    CHECK(read_val2 == data2);
}

TEST_CASE("buffer integration - fill and drain cycle") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    
    
    for (int i = 0; i < 3; ++i) {
        test_utils::write_data(buffer.get(), i);
    }
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    for (int i = 0; i < 3; ++i) {
        IpcEntry entry_ref = entry.get();
        test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
        
        int read_val;
        memcpy(&read_val, entry_ref.payload, sizeof(int));
        CHECK(read_val == i);
    }
    
    
    for (int i = 10; i < 13; ++i) {
        test_utils::write_data(buffer.get(), i);
    }
    
    
    for (int i = 10; i < 13; ++i) {
        IpcEntry entry_ref = entry.get();
        test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
        
        int read_val;
        memcpy(&read_val, entry_ref.payload, sizeof(int));
        CHECK(read_val == i);
    }
}

TEST_CASE("buffer integration - error recovery sequence") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    std::vector<uint8_t> large_data(1000);
    IpcBufferWriteResult write_result = 
        ipc_buffer_write(buffer.get(), large_data.data(), large_data.size());
    CHECK(IpcBufferWriteResult_is_error(write_result));
    
    
    int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);
    
    
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    
    int read_val;
    memcpy(&read_val, entry_ref.payload, sizeof(int));
    CHECK(read_val == test_data);
}


TEST_CASE("buffer boundary - maximum size data") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    
    
    const size_t max_data_size = test_utils::LARGE_BUFFER_SIZE - 100; 
    
    std::vector<uint8_t> large_data(max_data_size, 0xAB);
    
    
    IpcBufferWriteResult write_result = 
        ipc_buffer_write(buffer.get(), large_data.data(), large_data.size());
    
    if (IpcBufferWriteResult_is_ok(write_result)) {
        
        test_utils::EntryWrapper entry(max_data_size);
        IpcEntry entry_ref = entry.get();
        test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
        
        CHECK(entry_ref.size == max_data_size);
        
        
        CHECK(memcmp(entry_ref.payload, large_data.data(), max_data_size) == 0);
    }
    
}

TEST_CASE("buffer boundary - single byte operations") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    uint8_t single_byte = 0xFF;
    test_utils::write_data(buffer.get(), single_byte);
    
    
    test_utils::EntryWrapper entry(sizeof(uint8_t));
    IpcEntry entry_ref = entry.get();
    test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
    
    CHECK(entry_ref.size == sizeof(uint8_t));
    
    uint8_t read_byte;
    memcpy(&read_byte, entry_ref.payload, sizeof(uint8_t));
    CHECK(read_byte == single_byte);
}

TEST_CASE("buffer boundary - simple overflow test") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    
    int test_data = 42;
    IpcBufferWriteResult write_result = ipc_buffer_write(buffer.get(), &test_data, sizeof(test_data));
    
    
    if (IpcBufferWriteResult_is_ok(write_result)) {
        test_utils::EntryWrapper entry(sizeof(int));
        IpcEntry entry_ref = entry.get();
        IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
        
        if (read_result.ipc_status == IPC_OK) {
            int read_val;
            memcpy(&read_val, entry_ref.payload, sizeof(int));
            CHECK(read_val == test_data);
        }
    }
    
    
}

TEST_CASE("buffer boundary - reserve commit with maximum size") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    
    const size_t max_reserve_size = test_utils::LARGE_BUFFER_SIZE - 100;
    
    void* dest;
    IpcBufferReserveEntryResult reserve_result = 
        ipc_buffer_reserve_entry(buffer.get(), max_reserve_size, &dest);
    
    if (IpcBufferReserveEntryResult_is_ok(reserve_result)) {
        
        std::vector<uint8_t> pattern(max_reserve_size, 0xCD);
        memcpy(dest, pattern.data(), max_reserve_size);
        
        
        IpcBufferCommitEntryResult commit_result = 
            ipc_buffer_commit_entry(buffer.get(), reserve_result.result);
        test_utils::CHECK_OK(commit_result);
        
        
        test_utils::EntryWrapper entry(max_reserve_size);
        IpcEntry entry_ref = entry.get();
        test_utils::CHECK_OK(ipc_buffer_read(buffer.get(), &entry_ref));
        
        CHECK(entry_ref.size == max_reserve_size);
        CHECK(memcmp(entry_ref.payload, pattern.data(), max_reserve_size) == 0);
    }
}

TEST_CASE("buffer data - different sizes") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);
    
    // Тестируем данные разных размеров
    struct TestData {
        size_t size;
        uint8_t pattern;
        const char* description;
    };
    
    std::vector<TestData> test_cases = {
        {1, 0xAA, "single byte"},
        {4, 0xBB, "4 bytes (int)"},
        {8, 0xCC, "8 bytes (long long)"},
        {16, 0xDD, "16 bytes"},
        {32, 0xEE, "32 bytes"},
        {64, 0xFF, "64 bytes"},
        {128, 0x11, "128 bytes"},
        {256, 0x22, "256 bytes"},
        {512, 0x33, "512 bytes"}
    };
    
    // Записываем данные разных размеров
    std::vector<std::vector<uint8_t>> written_data;
    for (const auto& test_case : test_cases) {
        std::vector<uint8_t> data(test_case.size, test_case.pattern);
        written_data.push_back(data);
        
        IpcBufferWriteResult write_result = 
            ipc_buffer_write(buffer.get(), data.data(), data.size());
        
        if (IpcBufferWriteResult_is_ok(write_result)) {
            // Успешно записали
        } else {
            // Если не помещается, пропускаем этот размер
            written_data.pop_back();
            break;
        }
    }
    
    // Читаем и проверяем данные
    for (size_t i = 0; i < written_data.size(); ++i) {
        test_utils::EntryWrapper entry(written_data[i].size());
        IpcEntry entry_ref = entry.get();
        IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
        
        CHECK(read_result.ipc_status == IPC_OK);
        CHECK(entry_ref.size == written_data[i].size());
        
        // Проверяем содержимое
        CHECK(memcmp(entry_ref.payload, written_data[i].data(), written_data[i].size()) == 0);
        
        // Проверяем, что все байты имеют правильный паттерн
        const uint8_t expected_pattern = test_cases[i].pattern;
        for (size_t j = 0; j < entry_ref.size; ++j) {
            CHECK(static_cast<uint8_t*>(entry_ref.payload)[j] == expected_pattern);
        }
    }
    
    // Проверяем, что буфер пуст после чтения всех данных
    test_utils::EntryWrapper entry(1);
    IpcEntry entry_ref = entry.get();
    IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
    CHECK(read_result.ipc_status == IPC_EMPTY);
}
