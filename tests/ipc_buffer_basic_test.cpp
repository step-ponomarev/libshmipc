#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "shmipc/ipc_common.h"
#include "shmipc/ipc_buffer.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

TEST_CASE("create too small buffer") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result = ipc_buffer_create(mem, 0);
    CHECK(IpcBufferCreateResult_is_error(buffer_result));
    CHECK(buffer_result.ipc_status == IPC_ERR_INVALID_ARGUMENT);
}

TEST_CASE("size align function") {
    uint8_t mem[128];
    const IpcBufferCreateResult buffer_result =
        ipc_buffer_create(mem, ipc_buffer_align_size(0));
    CHECK(IpcBufferCreateResult_is_ok(buffer_result));
}

TEST_CASE("single entry") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int eval = 12;

    CHECK(IpcBufferWriteResult_is_ok(
        ipc_buffer_write(buffer, &eval, sizeof(eval))));

    IpcEntry entry = {.payload = malloc(sizeof(eval)), .size = sizeof(eval)};

    const IpcBufferReadResult result = ipc_buffer_read(buffer, &entry);
    CHECK(IpcBufferReadResult_is_ok(result));
    CHECK(entry.size == sizeof(eval));

    int res;
    memcpy(&res, entry.payload, entry.size);

    CHECK(res == eval);

    free(entry.payload);
    free(buffer);
}

TEST_CASE("fill buffer") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    size_t added_count = 0;
    while (IpcBufferWriteResult_is_ok(
               ipc_buffer_write(buffer, &added_count, sizeof(size_t))) &&
           (++added_count))
        ;

    const IpcBufferWriteResult status_result =
        ipc_buffer_write(buffer, &added_count, sizeof(size_t));
    CHECK(IpcBufferWriteResult_is_error(status_result));
    CHECK(status_result.ipc_status == IPC_ERR_NO_SPACE_CONTIGUOUS);

    size_t *ptr = static_cast<size_t*>(malloc(sizeof(size_t)));
    IpcEntry entry = {.payload = ptr, .size = sizeof(size_t)};
    for (size_t i = 0; i < added_count; i++) {
        const IpcBufferReadResult read_res = ipc_buffer_read(buffer, &entry);
        CHECK(IpcBufferReadResult_is_ok(read_res));
        CHECK(read_res.ipc_status == IPC_OK);
        CHECK(entry.size == sizeof(size_t));

        size_t res;
        memcpy(&res, entry.payload, entry.size);

        CHECK(res == i);
    }

    const IpcBufferReadResult read_res = ipc_buffer_read(buffer, &entry);
    CHECK(IpcBufferReadResult_is_ok(read_res));
    CHECK(read_res.ipc_status == IPC_EMPTY);

    free(ptr);
    free(buffer);
}

TEST_CASE("add to full buffer") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    size_t added_count = 0;
    while (IpcBufferWriteResult_is_ok(
               ipc_buffer_write(buffer, &added_count, sizeof(size_t))) &&
           (++added_count))
        ;

    CHECK(ipc_buffer_write(buffer, &added_count, sizeof(size_t)).ipc_status ==
          IPC_ERR_NO_SPACE_CONTIGUOUS);

    free(buffer);
}

TEST_CASE("wrap buffer") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    size_t added_count = 0;
    while (ipc_buffer_write(buffer, &added_count, sizeof(size_t)).ipc_status ==
               IPC_OK &&
           (++added_count))
        ;

    CHECK(ipc_buffer_write(buffer, &added_count, sizeof(size_t)).ipc_status ==
          IPC_ERR_NO_SPACE_CONTIGUOUS);

    CHECK(ipc_buffer_skip_force(buffer).ipc_status == IPC_OK);

    const size_t last_val = 666;
    CHECK(ipc_buffer_write(buffer, &last_val, sizeof(last_val)).ipc_status ==
          IPC_OK);

    IpcEntry entry = {.payload = malloc(sizeof(size_t)), .size = sizeof(size_t)};

    size_t prev;

    while (ipc_buffer_read(buffer, &entry).ipc_status == IPC_OK) {
        CHECK(entry.size == sizeof(size_t));
        memcpy(&prev, entry.payload, entry.size);
    }

    CHECK(prev == last_val);
    free(entry.payload);
    free(buffer);
}

TEST_CASE("peek") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    CHECK(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val))
              .ipc_status == IPC_OK);

    IpcEntry entry;
    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_OK);
    CHECK(entry.size == sizeof(expected_val));

    int val;
    memcpy(&val, entry.payload, sizeof(expected_val));
    CHECK(expected_val == val);

    entry.payload = malloc(sizeof(expected_val));
    entry.size = sizeof(expected_val);

    CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_OK);
    memcpy(&val, entry.payload, sizeof(expected_val));
    CHECK(expected_val == val);

    free(entry.payload);

    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_EMPTY);

    free(buffer);
}

TEST_CASE("skip") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    CHECK(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val))
              .ipc_status == IPC_OK);

    IpcEntry entry;

    const IpcBufferPeekResult peek_res = ipc_buffer_peek(buffer, &entry);
    CHECK(peek_res.ipc_status == IPC_OK);
    CHECK(ipc_buffer_skip(buffer, entry.offset).ipc_status == IPC_OK);
    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_EMPTY);

    free(buffer);
}

TEST_CASE("double skip") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    CHECK(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val))
              .ipc_status == IPC_OK);

    IpcEntry entry;

    const IpcBufferPeekResult peek_res = ipc_buffer_peek(buffer, &entry);
    CHECK(peek_res.ipc_status == IPC_OK);
    CHECK(ipc_buffer_skip(buffer, entry.offset).ipc_status == IPC_OK);

    const IpcBufferSkipResult skip_result = ipc_buffer_skip(buffer, entry.offset);
    CHECK(IpcBufferSkipResult_is_error(skip_result));
    CHECK(skip_result.ipc_status == IPC_ERR_OFFSET_MISMATCH);
    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_EMPTY);

    free(buffer);
}

TEST_CASE("skip forced") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    CHECK(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val))
              .ipc_status == IPC_OK);

    IpcEntry entry;
    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_OK);
    CHECK(ipc_buffer_skip_force(buffer).ipc_status == IPC_OK);
    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_EMPTY);

    free(buffer);
}

TEST_CASE("skip with incorrect id") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    CHECK(ipc_buffer_write(buffer, &expected_val, sizeof(expected_val))
              .ipc_status == IPC_OK);

    IpcEntry entry;

    const IpcBufferPeekResult result = ipc_buffer_peek(buffer, &entry);
    CHECK(result.ipc_status == IPC_OK);

    IpcEntry entry2;
    const IpcBufferPeekResult result2 = ipc_buffer_peek(buffer, &entry2);
    CHECK(result2.ipc_status == IPC_OK);
    CHECK(entry.offset == entry2.offset);

    CHECK(entry.size == entry2.size);

    int val1;
    memcpy(&val1, entry.payload, entry.size);

    int val2;
    memcpy(&val2, entry2.payload, entry2.size);

    CHECK(val1 == val2);

    free(buffer);
}

TEST_CASE("peek consistency") {
    uint8_t mem[256];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 256);
    IpcBuffer *buffer = bufferResult.result;

    int v1 = 1, v2 = 2;
    CHECK(ipc_buffer_write(buffer, &v1, sizeof(v1)).ipc_status == IPC_OK);
    CHECK(ipc_buffer_write(buffer, &v2, sizeof(v2)).ipc_status == IPC_OK);

    IpcEntry entry;
    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_OK);

    int seen;
    memcpy(&seen, entry.payload, sizeof(seen));
    CHECK(seen == v1);

    CHECK(ipc_buffer_skip_force(buffer).ipc_status == IPC_OK);

    CHECK(ipc_buffer_peek(buffer, &entry).ipc_status == IPC_OK);
    memcpy(&seen, entry.payload, sizeof(seen));
    CHECK(seen == v2);

    free(buffer);
}

TEST_CASE("read too small") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    int val = 42;
    CHECK(ipc_buffer_write(buffer, &val, sizeof(val)).ipc_status == IPC_OK);

    IpcEntry entry = {.payload = malloc(sizeof(val) - 1),
                      .size = sizeof(val) - 1};

    CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_ERR_TOO_SMALL);

    free(entry.payload);
    free(buffer);
}

TEST_CASE("reserve commit read") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    int *data;

    const IpcBufferReserveEntryResult result =
        ipc_buffer_reserve_entry(buffer, sizeof(expected_val), reinterpret_cast<void **>(&data));
    CHECK(result.ipc_status == IPC_OK);

    IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                      .size = sizeof(expected_val)};

    CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_ERR_NOT_READY);

    *data = expected_val;
    CHECK(ipc_buffer_commit_entry(buffer, result.result).ipc_status == IPC_OK);
    CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_OK);
    CHECK(entry.size == sizeof(expected_val));

    int res;
    memcpy(&res, entry.payload, entry.size);
    CHECK(res == expected_val);

    free(entry.payload);
    free(buffer);
}

TEST_CASE("reserve double commit") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int expected_val = 12;
    int *data;

    const IpcBufferReserveEntryResult result =
        ipc_buffer_reserve_entry(buffer, sizeof(expected_val), reinterpret_cast<void **>(&data));
    CHECK(result.ipc_status == IPC_OK);

    IpcEntry entry = {.payload = malloc(sizeof(expected_val)),
                      .size = sizeof(expected_val)};

    CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_ERR_NOT_READY);

    *data = expected_val;
    CHECK(ipc_buffer_commit_entry(buffer, result.result).ipc_status == IPC_OK);
    CHECK(IpcBufferCommitEntryResult_is_error(
        ipc_buffer_commit_entry(buffer, result.result)));
    CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_OK);
    CHECK(entry.size == sizeof(expected_val));

    int res;
    memcpy(&res, entry.payload, entry.size);
    CHECK(res == expected_val);

    free(entry.payload);
    free(buffer);
}

TEST_CASE("multiple reserve commit read") {
    uint8_t mem[1024];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 1024);
    IpcBuffer *buffer = bufferResult.result;

    for (int i = 0; i < 10; ++i) {
        int *ptr;
        const IpcBufferReserveEntryResult result =
            ipc_buffer_reserve_entry(buffer, sizeof(int), reinterpret_cast<void **>(&ptr));

        CHECK(result.ipc_status == IPC_OK);
        *ptr = i;
        CHECK(ipc_buffer_commit_entry(buffer, result.result).ipc_status == IPC_OK);
    }

    int *buf = static_cast<int*>(malloc(sizeof(int)));
    IpcEntry entry = {.payload = buf, .size = sizeof(int)};
    for (int i = 0; i < 10; ++i) {
        CHECK(ipc_buffer_read(buffer, &entry).ipc_status == IPC_OK);
        CHECK(*buf == i);
    }

    free(buf);
    free(buffer);
}
