#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "shmipc/ipc_buffer.h"
#include "test_utils.h"
#include <cstring>

TEST_CASE("buffer create - too small size") {
    uint8_t mem[128];

    ipc_buffer_t *res = nullptr;
    ipc_error_t err;
    const size_t small_size = 0;
    const uint64_t min_size = ipc_buffer_get_min_size();
    const ipc_status_t status = ipc_buffer_create(mem, small_size, &res, &err);
    test_utils::CHECK_TOO_SMALL_SIZE_WITH_SIZE_ERROR(
        status, err, small_size, min_size, min_size);
    CHECK(res == nullptr);
}

TEST_CASE("buffer create - null memory pointer") {
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_create(nullptr, ipc_buffer_suggest_size(128), &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);
}

TEST_CASE("buffer suggest_size - valid size calculation") {
    uint8_t mem[128];

    ipc_buffer_t *res = nullptr;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_create(mem, ipc_buffer_suggest_size(0), &res, &err);
    CHECK(status == IPC_STATUS_OK);

    free(res);
}

TEST_CASE("buffer create - null out") {
    uint8_t mem[128];

    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_create(mem, 128, nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer create - invalid capacity not power of 2") {
    uint8_t mem[256];

    // size such that (size - header) is not power of 2
    const size_t bad_size = ipc_buffer_get_min_size() + 1;
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_create(mem, bad_size, &res, &err);
    test_utils::CHECK_INVALID_CAPACITY_ARG_ERROR(status, err);
    CHECK(res == nullptr);
}

TEST_CASE("buffer create - out is zeroed on error") {
    ipc_buffer_t *out = reinterpret_cast<ipc_buffer_t *>(0xBAD);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_create(nullptr, ipc_buffer_suggest_size(128), &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("buffer create - error reset after failed then success") {
    uint8_t mem[512];
    const size_t size = ipc_buffer_suggest_size(128);
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    ipc_status_t status = ipc_buffer_create(nullptr, size, &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);

    status = ipc_buffer_create(mem, size, &res, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(res != nullptr);

    free(res);
}

TEST_CASE("buffer create - success case") {
    const size_t size = ipc_buffer_suggest_size(test_utils::SMALL_BUFFER_SIZE);
    uint8_t mem[512]; // Large enough for suggested size

    ipc_buffer_t *buffer = nullptr;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_create(mem, size, &buffer, &err);
    CHECK(status == IPC_STATUS_OK);

    test_utils::verify_buffer_creation(buffer, size);

    free(buffer);
}

TEST_CASE("buffer attach - null out") {
    uint8_t mem[512];
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_attach(mem, nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer attach - null memory") {
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_attach(nullptr, &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);
}

TEST_CASE("buffer attach - out is null on error") {
    ipc_buffer_t *out = reinterpret_cast<ipc_buffer_t *>(0xBAD);  // any non-null to verify *out is cleared on error
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_attach(nullptr, &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("buffer attach - error reset after failed then success") {
    const size_t size = ipc_buffer_suggest_size(128);
    uint8_t mem[512];
    ipc_buffer_t *created = nullptr;
    CHECK(ipc_buffer_create(mem, size, &created, nullptr) == IPC_STATUS_OK);

    ipc_buffer_t *attached = nullptr;
    ipc_error_t err;
    ipc_status_t status = ipc_buffer_attach(nullptr, &attached, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(attached == nullptr);

    status = ipc_buffer_attach(mem, &attached, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(attached != nullptr);

    free(created);
    free(attached);
}

TEST_CASE("buffer attach - success case") {
    const size_t size = ipc_buffer_suggest_size(128);
    uint8_t mem[512]; // Large enough for suggested size

    ipc_buffer_t *created_buffer = nullptr;
    const ipc_status_t create_status = ipc_buffer_create(mem, size, &created_buffer, nullptr);
    CHECK(create_status == IPC_STATUS_OK);

    const int test_value = 42;
    CHECK(ipc_buffer_write(created_buffer, &test_value, sizeof(test_value), nullptr) == IPC_STATUS_OK);

    ipc_buffer_t *attached_buffer = nullptr;
    const ipc_status_t attach_status = ipc_buffer_attach(mem, &attached_buffer, nullptr);
    CHECK(attach_status == IPC_STATUS_OK);

    test_utils::EntryWrapper entry(sizeof(test_value));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result =
            ipc_buffer_read(attached_buffer, &entry_ref);
    CHECK(read_result.ipc_status == IPC_OK);

    int read_value;
    memcpy(&read_value, entry_ref.payload, sizeof(test_value));
    CHECK(read_value == test_value);

    free(created_buffer);
    free(attached_buffer);
}

TEST_CASE("write with NULL buffer") {
    const int test_data = 42;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(nullptr, &test_data, sizeof(test_data), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("write with NULL data") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(buffer.get(), nullptr, sizeof(int), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("write with zero size") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(buffer.get(), &test_data, 0, &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_ZERO_SIZE);
}

TEST_CASE("write success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    CHECK(test_utils::write_data_safe(buffer.get(), test_data));
}

TEST_CASE("write error structure verification") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int test_data = 42;
    ipc_error_t err;

    ipc_status_t status =
            ipc_buffer_write(nullptr, &test_data, sizeof(test_data), &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_NULL_ARG);
    CHECK(err.message != nullptr);

    status = ipc_buffer_write(buffer.get(), nullptr, sizeof(test_data), &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_NULL_ARG);
    CHECK(err.message != nullptr);

    status = ipc_buffer_write(buffer.get(), &test_data, 0, &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_ZERO_SIZE);
    CHECK(err.message != nullptr);
}

TEST_CASE("read with NULL buffer") {
    test_utils::EntryWrapper entry(sizeof(int));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_result = ipc_buffer_read(nullptr, &entry_ref);
    test_utils::CHECK_ERROR(read_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("read with NULL dest") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const IpcBufferReadResult read_result =
            ipc_buffer_read(buffer.get(), nullptr);
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
    const IpcBufferReadResult null_buffer_result =
            ipc_buffer_read(nullptr, &entry_ref);
    CHECK(IpcBufferReadResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 0);

    const IpcBufferReadResult null_dest_result =
            ipc_buffer_read(buffer.get(), nullptr);
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
    const IpcBufferPeekResult peek_result =
            ipc_buffer_peek(buffer.get(), nullptr);
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
    const IpcBufferPeekResult peek_result2 =
            ipc_buffer_peek(buffer.get(), &entry2);
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
    const IpcBufferPeekResult null_buffer_result =
            ipc_buffer_peek(nullptr, &entry);
    CHECK(IpcBufferPeekResult_is_error(null_buffer_result));
    CHECK(null_buffer_result.error.body.offset == 0);

    const IpcBufferPeekResult null_dest_result =
            ipc_buffer_peek(buffer.get(), nullptr);
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

    const IpcBufferSkipResult skip_result =
            ipc_buffer_skip(buffer.get(), entry.offset);
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

    const IpcBufferSkipResult wrong_offset_result =
            ipc_buffer_skip(buffer.get(), 256);
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

    const IpcBufferSkipResult skip_result =
            ipc_buffer_skip(buffer.get(), original_offset);
    test_utils::CHECK_OK(skip_result);
    CHECK(skip_result.result == original_offset);
}

TEST_CASE("skip_force with NULL buffer") {
    const IpcBufferSkipForceResult skip_force_result =
            ipc_buffer_skip_force(nullptr);
    test_utils::CHECK_ERROR(skip_force_result, IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("skip_force success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);

    IpcEntry entry;
    const IpcBufferPeekResult peek_result = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek_result);

    const IpcBufferSkipForceResult skip_force_result =
            ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force_result);
    CHECK(skip_force_result.result == 0);

    const IpcBufferPeekResult peek_after = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
}

TEST_CASE("skip_force empty buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const IpcBufferSkipForceResult skip_force_result =
            ipc_buffer_skip_force(buffer.get());
    CHECK(skip_force_result.ipc_status == IPC_EMPTY);
    CHECK(skip_force_result.result == 0);
}

TEST_CASE("skip_force error structure verification") {
    const IpcBufferSkipForceResult null_buffer_result =
            ipc_buffer_skip_force(nullptr);
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

    const IpcBufferSkipForceResult skip_force1 =
            ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force1);

    const IpcBufferPeekResult peek2 = ipc_buffer_peek(buffer.get(), &entry);
    test_utils::CHECK_OK(peek2);

    int peeked_v2;
    memcpy(&peeked_v2, entry.payload, sizeof(v2));
    CHECK(peeked_v2 == v2);

    const IpcBufferSkipForceResult skip_force2 =
            ipc_buffer_skip_force(buffer.get());
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

    const IpcBufferSkipForceResult skip_force_result =
            ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force_result);
    CHECK(skip_force_result.result == 0);
}

TEST_CASE("skip_force vs skip comparison") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);

    const IpcBufferSkipForceResult skip_force_result =
            ipc_buffer_skip_force(buffer.get());
    test_utils::CHECK_OK(skip_force_result);

    IpcEntry entry;
    const IpcBufferPeekResult peek_after = ipc_buffer_peek(buffer.get(), &entry);
    CHECK(peek_after.ipc_status == IPC_EMPTY);
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
    while (ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t), nullptr) ==
               IPC_STATUS_OK &&
           (++added_count));

    const ipc_status_t status =
            ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t), nullptr);
    CHECK(status == IPC_STATUS_NO_SPACE);

    test_utils::EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < added_count; i++) {
        IpcEntry entry_ref = entry.get();
        const IpcBufferReadResult read_res =
                ipc_buffer_read(buffer.get(), &entry_ref);
        test_utils::CHECK_OK(read_res);
        CHECK(read_res.ipc_status == IPC_OK);
        CHECK(entry_ref.size == sizeof(size_t));

        size_t res;
        memcpy(&res, entry_ref.payload, entry_ref.size);
        CHECK(res == i);
    }

    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult read_res =
            ipc_buffer_read(buffer.get(), &entry_ref);
    CHECK(read_res.ipc_status == IPC_EMPTY);
}

TEST_CASE("add to full buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t added_count = 0;
    while (ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t), nullptr) ==
               IPC_STATUS_OK &&
           (++added_count));

    CHECK(ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t), nullptr) ==
        IPC_STATUS_NO_SPACE);
}

TEST_CASE("wrap buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t added_count = 0;
    while (
        ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t), nullptr) ==
        IPC_STATUS_OK &&
        (++added_count)) {
    }

    CHECK(
        ipc_buffer_write(buffer.get(), &added_count, sizeof(size_t), nullptr) ==
        IPC_STATUS_NO_SPACE);

    CHECK(ipc_buffer_skip_force(buffer.get()).ipc_status == IPC_OK);

    const size_t last_val = 666;
    CHECK(
        ipc_buffer_write(buffer.get(), &last_val, sizeof(last_val), nullptr) ==
        IPC_STATUS_OK);

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

    const IpcBufferSkipResult skip_result =
            ipc_buffer_skip(buffer.get(), entry.offset);
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
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref).ipc_status ==
        IPC_ERR_TOO_SMALL);
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

TEST_CASE("buffer integration - mixed operations sequence") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);

    int data1 = 100;
    test_utils::write_data(buffer.get(), data1);

    int data2 = 200;
    test_utils::write_data(buffer.get(), data2);

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
    ipc_error_t write_err;
    const ipc_status_t write_status =
            ipc_buffer_write(buffer.get(), large_data.data(), large_data.size(), &write_err);
    CHECK(write_status == IPC_STATUS_ERROR);
    CHECK(write_err.code == IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER);

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

    const ipc_status_t write_status =
            ipc_buffer_write(buffer.get(), large_data.data(), large_data.size(), nullptr);

    if (write_status == IPC_STATUS_OK) {
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
    const ipc_status_t write_status =
            ipc_buffer_write(buffer.get(), &test_data, sizeof(test_data), nullptr);

    if (write_status == IPC_STATUS_OK) {
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

TEST_CASE("buffer data - different sizes") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);

    struct TestData {
        size_t size;
        uint8_t pattern;
    };

    std::vector<TestData> test_cases = {
        {1, 0xAA}, {4, 0xBB}, {8, 0xCC},
        {16, 0xDD}, {32, 0xEE}, {64, 0xFF},
        {128, 0x11}, {256, 0x22}, {512, 0x33}
    };

    std::vector<std::vector<uint8_t> > written_data;
    for (const auto &test_case: test_cases) {
        std::vector<uint8_t> data(test_case.size, test_case.pattern);
        written_data.push_back(data);

        const ipc_status_t write_status =
                ipc_buffer_write(buffer.get(), data.data(), data.size(), nullptr);

        if (write_status != IPC_STATUS_OK) {
            written_data.pop_back();
            break;
        }
    }

    for (size_t i = 0; i < written_data.size(); ++i) {
        test_utils::EntryWrapper entry(written_data[i].size());
        IpcEntry entry_ref = entry.get();
        IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);

        CHECK(read_result.ipc_status == IPC_OK);
        CHECK(entry_ref.size == written_data[i].size());
        CHECK(memcmp(entry_ref.payload, written_data[i].data(),
            written_data[i].size()) == 0);
    }

    test_utils::EntryWrapper entry(1);
    IpcEntry entry_ref = entry.get();
    IpcBufferReadResult read_result = ipc_buffer_read(buffer.get(), &entry_ref);
    CHECK(read_result.ipc_status == IPC_EMPTY);
}
