#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "shmipc/ipc_buffer.h"
#include "test_utils.h"
#include "BufferWrapper.hpp"
#include <cstring>

TEST_CASE("buffer init - null out") {
    uint8_t mem[128];

    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_init(mem, 128, nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer init - null memory") {
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_init(nullptr, ipc_buffer_suggest_size(128), &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);
}

TEST_CASE("buffer init - too small size") {
    uint8_t mem[128];

    ipc_buffer_t *res = nullptr;
    ipc_error_t err;
    const size_t small_size = 0;
    const uint64_t min_size = ipc_buffer_min_size();

    const ipc_status_t status = ipc_buffer_init(mem, small_size, &res, &err);
    test_utils::CHECK_SIZE_ERROR(
        status, err, IPC_ERR_CODE_TOO_SMALL_SIZE, small_size, min_size, min_size);
    CHECK(res == nullptr);
}

TEST_CASE("buffer init - invalid capacity not power of 2") {
    uint8_t mem[256];

    const size_t bad_size = ipc_buffer_min_size() + 1;
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_init(mem, bad_size, &res, &err);
    test_utils::CHECK_INVALID_CAPACITY_ARG_ERROR(status, err);
    CHECK(res == nullptr);
}

TEST_CASE("buffer init - out is zeroed on error") {
    ipc_buffer_t *out = reinterpret_cast<ipc_buffer_t *>(0xBAD);
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_init(nullptr, ipc_buffer_suggest_size(128), &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("buffer init - error reset after failed then success") {
    const size_t size = ipc_buffer_suggest_size(128);
    std::vector<uint8_t> mem(size);
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    ipc_status_t status = ipc_buffer_init(nullptr, size, &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);

    status = ipc_buffer_init(mem.data(), size, &res, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(res != nullptr);

    ipc_buffer_detach(res, nullptr);
}

TEST_CASE("buffer init - success") {
    const size_t size = ipc_buffer_suggest_size(test_utils::SMALL_BUFFER_SIZE);
    std::vector<uint8_t> mem(size);

    ipc_buffer_t *buffer = nullptr;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_init(mem.data(), size, &buffer, &err);
    CHECK(status == IPC_STATUS_OK);
    CHECK(buffer != nullptr);

    ipc_buffer_detach(buffer, nullptr);
}

TEST_CASE("buffer suggest_size - produces valid init size") {
    const size_t size = ipc_buffer_suggest_size(0);
    std::vector<uint8_t> mem(size);

    ipc_buffer_t *res = nullptr;
    const ipc_status_t status = ipc_buffer_init(mem.data(), size, &res, nullptr);
    CHECK(status == IPC_STATUS_OK);

    ipc_buffer_detach(res, nullptr);
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

TEST_CASE("buffer attach - out is zeroed on error") {
    ipc_buffer_t *out = reinterpret_cast<ipc_buffer_t *>(0xBAD);
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_attach(nullptr, &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("buffer attach - error reset after failed then success") {
    const size_t size = ipc_buffer_suggest_size(128);
    std::vector<uint8_t> mem(size);
    ipc_buffer_t *created = nullptr;
    CHECK(ipc_buffer_init(mem.data(), size, &created, nullptr) == IPC_STATUS_OK);

    ipc_buffer_t *attached = nullptr;
    ipc_error_t err;
    ipc_status_t status = ipc_buffer_attach(nullptr, &attached, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(attached == nullptr);

    status = ipc_buffer_attach(mem.data(), &attached, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(attached != nullptr);

    ipc_buffer_detach(created, nullptr);
    ipc_buffer_detach(attached, nullptr);
}

TEST_CASE("buffer attach - reads data written by creator") {
    const size_t size = ipc_buffer_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_buffer_t *created = nullptr;
    CHECK(ipc_buffer_init(mem.data(), size, &created, nullptr) == IPC_STATUS_OK);

    const int test_value = 42;
    CHECK(ipc_buffer_write(created, &test_value, sizeof(test_value), nullptr) == IPC_STATUS_OK);

    ipc_buffer_t *attached = nullptr;
    CHECK(ipc_buffer_attach(mem.data(), &attached, nullptr) == IPC_STATUS_OK);

    const int read_val = test_utils::read_data<int>(attached);
    CHECK(read_val == test_value);

    ipc_buffer_detach(created, nullptr);
    ipc_buffer_detach(attached, nullptr);
}

TEST_CASE("buffer detach - null buffer") {
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_detach(nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer detach - success") {
    const size_t size = ipc_buffer_suggest_size(128);
    std::vector<uint8_t> mem(size);

    ipc_buffer_t *buffer = nullptr;
    CHECK(ipc_buffer_init(mem.data(), size, &buffer, nullptr) == IPC_STATUS_OK);

    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_detach(buffer, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
}

TEST_CASE("buffer write - null buffer") {
    const int data = 42;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_write(nullptr, &data, sizeof(data), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer write - null data") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_write(buffer.get(), nullptr, sizeof(int), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer write - zero size") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int data = 42;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_write(buffer.get(), &data, 0, &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.kind == IPC_ERR_KIND_ARG);
    CHECK(err.code == IPC_ERR_CODE_ZERO_SIZE);
    CHECK(err.message != nullptr);
}

TEST_CASE("buffer write - size exceeds buffer") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    std::vector<uint8_t> large_data(test_utils::SMALL_BUFFER_SIZE * 10, 0xAB);

    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(buffer.get(), large_data.data(), large_data.size(), &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.kind == IPC_ERR_KIND_ARG);
    CHECK(err.code == IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER);
    CHECK(err.message != nullptr);
    CHECK(err.as.arg.size.requested_size > err.as.arg.size.limit);
    CHECK(err.as.arg.size.limit > 0);
    CHECK(err.as.arg.size.suggested_size == err.as.arg.size.limit);
}

TEST_CASE("buffer write - no space") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t count = 0;
    while (ipc_buffer_write(buffer.get(), &count, sizeof(count), nullptr) == IPC_STATUS_OK) {
        ++count;
    }

    const ipc_status_t status = ipc_buffer_write(buffer.get(), &count, sizeof(count), nullptr);
    CHECK(status == IPC_STATUS_NO_SPACE);
}

TEST_CASE("buffer write - success") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int data = 42;
    CHECK(test_utils::write_data_safe(buffer.get(), data));
}

TEST_CASE("buffer read - null buffer") {
    EntryWrapper entry(sizeof(int));
    ipc_entry_t entry_ref = entry.get();
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_read(nullptr, &entry_ref, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer read - null dest") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_read(buffer.get(), nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer read - empty buffer") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    EntryWrapper entry(sizeof(int));
    ipc_entry_t entry_ref = entry.get();
    const ipc_status_t status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);
    CHECK(status == IPC_STATUS_EMPTY);
}

TEST_CASE("buffer read - dest too small") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    int val = 42;
    test_utils::write_data(buffer.get(), val);

    EntryWrapper entry(sizeof(val) - 1);
    ipc_entry_t entry_ref = entry.get();
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_read(buffer.get(), &entry_ref, &err);
    test_utils::CHECK_CAPACITY_ERROR(status, err, sizeof(val) - 1, sizeof(val));
}

TEST_CASE("buffer read - dest too small does not consume entry") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    int val = 42;
    test_utils::write_data(buffer.get(), val);

    EntryWrapper small_entry(sizeof(val) - 1);
    ipc_entry_t small_ref = small_entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &small_ref, nullptr) == IPC_STATUS_ERROR);

    EntryWrapper entry(sizeof(val));
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

    int result;
    memcpy(&result, entry_ref.payload, sizeof(int));
    CHECK(result == val);
}

TEST_CASE("buffer read - dest larger than payload") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int data = 42;
    test_utils::write_data(buffer.get(), data);

    EntryWrapper entry(sizeof(int) * 4);
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
    CHECK(entry_ref.size == sizeof(int));

    int result;
    memcpy(&result, entry_ref.payload, sizeof(int));
    CHECK(result == data);
}

TEST_CASE("buffer read - success") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int data = 42;
    test_utils::write_data(buffer.get(), data);

    const int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == data);
}

TEST_CASE("buffer next_entry_size - null buffer") {
    size_t out_size = 0;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_next_entry_size(nullptr, &out_size, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer next_entry_size - null out_size") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_next_entry_size(buffer.get(), nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer next_entry_size - empty buffer") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    size_t out_size = 999;
    const ipc_status_t status = ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr);
    CHECK(status == IPC_STATUS_EMPTY);
    CHECK(out_size == 0);
}

TEST_CASE("buffer next_entry_size - success") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int data = 42;
    test_utils::write_data(buffer.get(), data);

    size_t out_size = 0;
    const ipc_status_t status = ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr);
    CHECK(status == IPC_STATUS_OK);
    CHECK(out_size == sizeof(int));
}

TEST_CASE("buffer next_entry_size - does not consume entry") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int data = 42;
    test_utils::write_data(buffer.get(), data);

    size_t out_size = 0;
    CHECK(ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr) == IPC_STATUS_OK);
    CHECK(out_size == sizeof(int));

    CHECK(ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr) == IPC_STATUS_OK);
    CHECK(out_size == sizeof(int));

    const int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == data);
}

TEST_CASE("buffer next_entry_size - skips placeholder") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t count = 0;
    while (ipc_buffer_write(buffer.get(), &count, sizeof(count), nullptr) != IPC_STATUS_NO_SPACE) {
        ++count;
    }
    CHECK(count > 1);

    EntryWrapper drain_entry(sizeof(size_t));
    ipc_entry_t drain_ref = drain_entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &drain_ref, nullptr) == IPC_STATUS_OK);

    while (true) {
        ipc_entry_t ref = drain_entry.get();
        if (ipc_buffer_read(buffer.get(), &ref, nullptr) != IPC_STATUS_OK) break;
    }

    const int val = 777;
    CHECK(ipc_buffer_write(buffer.get(), &val, sizeof(val), nullptr) == IPC_STATUS_OK);

    size_t out_size = 0;
    CHECK(ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr) == IPC_STATUS_OK);
    CHECK(out_size == sizeof(int));
}

TEST_CASE("buffer data - single byte") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    uint8_t byte = 0xFF;
    test_utils::write_data(buffer.get(), byte);

    EntryWrapper entry(sizeof(uint8_t));
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
    CHECK(entry_ref.size == sizeof(uint8_t));

    uint8_t read_byte;
    memcpy(&read_byte, entry_ref.payload, sizeof(uint8_t));
    CHECK(read_byte == byte);
}

TEST_CASE("buffer data - different sizes") {
    BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);

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
    for (const auto &tc: test_cases) {
        std::vector data(tc.size, tc.pattern);
        written_data.push_back(data);

        if (ipc_buffer_write(buffer.get(), data.data(), data.size(), nullptr) != IPC_STATUS_OK) {
            written_data.pop_back();
            break;
        }
    }

    for (size_t i = 0; i < written_data.size(); ++i) {
        EntryWrapper entry(written_data[i].size());
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
        CHECK(entry_ref.size == written_data[i].size());
        CHECK(memcmp(entry_ref.payload, written_data[i].data(), written_data[i].size()) == 0);
    }

    EntryWrapper entry(1);
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_EMPTY);
}

TEST_CASE("buffer data - fill and drain") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t count = 0;
    while (ipc_buffer_write(buffer.get(), &count, sizeof(count), nullptr) != IPC_STATUS_NO_SPACE) {
        ++count;
    }
    CHECK(count > 0);

    EntryWrapper entry(sizeof(size_t));
    for (size_t i = 0; i < count; i++) {
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
        CHECK(entry_ref.size == sizeof(size_t));

        size_t res;
        memcpy(&res, entry_ref.payload, entry_ref.size);
        CHECK(res == i);
    }

    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_EMPTY);
}

TEST_CASE("buffer data - fill drain refill") {
    BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

    EntryWrapper entry(sizeof(int));

    for (int i = 0; i < 3; ++i) {
        test_utils::write_data(buffer.get(), i);
    }

    for (int i = 0; i < 3; ++i) {
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

        int val;
        memcpy(&val, entry_ref.payload, sizeof(int));
        CHECK(val == i);
    }

    for (int i = 10; i < 13; ++i) {
        test_utils::write_data(buffer.get(), i);
    }

    for (int i = 10; i < 13; ++i) {
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

        int val;
        memcpy(&val, entry_ref.payload, sizeof(int));
        CHECK(val == i);
    }
}

TEST_CASE("buffer data - wrap around") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    size_t count = 0;
    while (ipc_buffer_write(buffer.get(), &count, sizeof(count), nullptr) == IPC_STATUS_OK) {
        ++count;
    }
    CHECK(ipc_buffer_write(buffer.get(), &count, sizeof(count), nullptr) == IPC_STATUS_NO_SPACE);

    EntryWrapper drain_entry(sizeof(size_t));
    ipc_entry_t drain_ref = drain_entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &drain_ref, nullptr) == IPC_STATUS_OK);

    const size_t wrap_val = 666;
    CHECK(ipc_buffer_write(buffer.get(), &wrap_val, sizeof(wrap_val), nullptr) == IPC_STATUS_OK);

    EntryWrapper entry(sizeof(size_t));
    size_t last = 0;
    while (true) {
        ipc_entry_t entry_ref = entry.get();
        if (ipc_buffer_read(buffer.get(), &entry_ref, nullptr) != IPC_STATUS_OK) break;
        memcpy(&last, entry_ref.payload, entry_ref.size);
    }

    CHECK(last == wrap_val);
}

TEST_CASE("buffer data - next_entry_size then read") {
    BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);
    int data = 42;
    test_utils::write_data(buffer.get(), data);

    size_t next_size = 0;
    CHECK(ipc_buffer_next_entry_size(buffer.get(), &next_size, nullptr) == IPC_STATUS_OK);
    CHECK(next_size == sizeof(int));

    EntryWrapper entry(next_size);
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

    int result;
    memcpy(&result, entry_ref.payload, sizeof(int));
    CHECK(result == data);

    CHECK(ipc_buffer_next_entry_size(buffer.get(), &next_size, nullptr) == IPC_STATUS_EMPTY);
}

TEST_CASE("buffer data - error recovery") {
    BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    std::vector<uint8_t> large_data(1000);
    ipc_error_t err;
    CHECK(ipc_buffer_write(buffer.get(), large_data.data(), large_data.size(), &err) == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER);

    int data = 42;
    test_utils::write_data(buffer.get(), data);

    const int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == data);
}
