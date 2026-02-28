#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "shmipc/ipc_buffer.h"
#include "test_utils.h"
#include <cstring>

// ── init ──

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

TEST_CASE("buffer init - null memory pointer") {
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    const ipc_status_t status = ipc_buffer_init(nullptr, ipc_buffer_suggest_size(128), &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);
}

TEST_CASE("buffer suggest_size - valid size calculation") {
    uint8_t mem[128];

    ipc_buffer_t *res = nullptr;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_init(mem, ipc_buffer_suggest_size(0), &res, &err);
    CHECK(status == IPC_STATUS_OK);

    ipc_buffer_detach(res, nullptr);
}

TEST_CASE("buffer init - null out") {
    uint8_t mem[128];

    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_init(mem, 128, nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
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
    uint8_t mem[512];
    const size_t size = ipc_buffer_suggest_size(128);
    ipc_buffer_t *res = nullptr;
    ipc_error_t err;

    ipc_status_t status = ipc_buffer_init(nullptr, size, &res, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(res == nullptr);

    status = ipc_buffer_init(mem, size, &res, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(res != nullptr);

    ipc_buffer_detach(res, nullptr);
}

TEST_CASE("buffer init - success case") {
    const size_t size = ipc_buffer_suggest_size(test_utils::SMALL_BUFFER_SIZE);
    uint8_t mem[512];

    ipc_buffer_t *buffer = nullptr;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_init(mem, size, &buffer, &err);
    CHECK(status == IPC_STATUS_OK);

    test_utils::verify_buffer_creation(buffer, size);

    ipc_buffer_detach(buffer, nullptr);
}

// ── attach ──

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
    ipc_buffer_t *out = reinterpret_cast<ipc_buffer_t *>(0xBAD);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_attach(nullptr, &out, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(out == nullptr);
}

TEST_CASE("buffer attach - error reset after failed then success") {
    const size_t size = ipc_buffer_suggest_size(128);
    uint8_t mem[512];
    ipc_buffer_t *created = nullptr;
    CHECK(ipc_buffer_init(mem, size, &created, nullptr) == IPC_STATUS_OK);

    ipc_buffer_t *attached = nullptr;
    ipc_error_t err;
    ipc_status_t status = ipc_buffer_attach(nullptr, &attached, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
    CHECK(attached == nullptr);

    status = ipc_buffer_attach(mem, &attached, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
    CHECK(attached != nullptr);

    ipc_buffer_detach(created, nullptr);
    ipc_buffer_detach(attached, nullptr);
}

TEST_CASE("buffer attach - success case") {
    const size_t size = ipc_buffer_suggest_size(128);
    uint8_t mem[512];

    ipc_buffer_t *created_buffer = nullptr;
    const ipc_status_t create_status = ipc_buffer_init(mem, size, &created_buffer, nullptr);
    CHECK(create_status == IPC_STATUS_OK);

    const int test_value = 42;
    CHECK(ipc_buffer_write(created_buffer, &test_value, sizeof(test_value), nullptr) == IPC_STATUS_OK);

    ipc_buffer_t *attached_buffer = nullptr;
    const ipc_status_t attach_status = ipc_buffer_attach(mem, &attached_buffer, nullptr);
    CHECK(attach_status == IPC_STATUS_OK);

    test_utils::EntryWrapper entry(sizeof(test_value));
    ipc_entry_t entry_ref = entry.get();
    const ipc_status_t read_status = ipc_buffer_read(attached_buffer, &entry_ref, nullptr);
    CHECK(read_status == IPC_STATUS_OK);

    int read_value;
    memcpy(&read_value, entry_ref.payload, sizeof(test_value));
    CHECK(read_value == test_value);

    ipc_buffer_detach(created_buffer, nullptr);
    ipc_buffer_detach(attached_buffer, nullptr);
}

// ── detach ──

TEST_CASE("buffer detach - null buffer") {
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_detach(nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer detach - success case") {
    const size_t size = ipc_buffer_suggest_size(128);
    uint8_t mem[512];

    ipc_buffer_t *buffer = nullptr;
    CHECK(ipc_buffer_init(mem, size, &buffer, nullptr) == IPC_STATUS_OK);

    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_detach(buffer, &err);
    test_utils::CHECK_ERROR_NONE(status, err);
}

// ── write ──

TEST_CASE("buffer write - null buffer") {
    const int test_data = 42;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(nullptr, &test_data, sizeof(test_data), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer write - null data") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(buffer.get(), nullptr, sizeof(int), &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer write - zero size") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    ipc_error_t err;
    const ipc_status_t status =
            ipc_buffer_write(buffer.get(), &test_data, 0, &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.code == IPC_ERR_CODE_ZERO_SIZE);
}

TEST_CASE("buffer write - success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    CHECK(test_utils::write_data_safe(buffer.get(), test_data));
}

TEST_CASE("buffer write - size exceeds buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

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

// ── read ──

TEST_CASE("buffer read - null buffer") {
    test_utils::EntryWrapper entry(sizeof(int));
    ipc_entry_t entry_ref = entry.get();
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_read(nullptr, &entry_ref, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer read - null dest") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_read(buffer.get(), nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer read - success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);

    const int read_data = test_utils::read_data_safe<int>(buffer.get());
    CHECK(read_data == test_data);
}

TEST_CASE("buffer read - empty buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    test_utils::EntryWrapper entry(sizeof(int));
    ipc_entry_t entry_ref = entry.get();
    const ipc_status_t status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);
    CHECK(status == IPC_STATUS_EMPTY);
}

TEST_CASE("buffer read - dest too small") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    int val = 42;
    test_utils::write_data(buffer.get(), val);

    test_utils::EntryWrapper entry(sizeof(val) - 1);
    ipc_entry_t entry_ref = entry.get();
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_read(buffer.get(), &entry_ref, &err);
    CHECK(status == IPC_STATUS_ERROR);
    CHECK(err.kind == IPC_ERR_KIND_ARG);
    CHECK(err.code == IPC_ERR_CODE_INVALID_CAPACITY);
    CHECK(err.message != nullptr);
    CHECK(err.as.arg.capacity.provided_capacity == sizeof(val) - 1);
    CHECK(err.as.arg.capacity.required_capacity == sizeof(val));
}

// ── next_size ──

TEST_CASE("buffer next_size - null buffer") {
    size_t out_size = 0;
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_next_entry_size(nullptr, &out_size, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer next_size - null out_size") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    ipc_error_t err;
    const ipc_status_t status = ipc_buffer_next_entry_size(buffer.get(), nullptr, &err);
    test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("buffer next_size - empty buffer") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    size_t out_size = 999;
    const ipc_status_t status = ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr);
    CHECK(status == IPC_STATUS_EMPTY);
    CHECK(out_size == 0);
}

TEST_CASE("buffer next_size - success case") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);

    size_t out_size = 0;
    const ipc_status_t status = ipc_buffer_next_entry_size(buffer.get(), &out_size, nullptr);
    CHECK(status == IPC_STATUS_OK);
    CHECK(out_size == sizeof(int));
}

TEST_CASE("buffer next_size - does not consume entry") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    const int test_data = 42;
    test_utils::write_data(buffer.get(), test_data);

    size_t out_size1 = 0;
    CHECK(ipc_buffer_next_entry_size(buffer.get(), &out_size1, nullptr) == IPC_STATUS_OK);
    CHECK(out_size1 == sizeof(int));

    size_t out_size2 = 0;
    CHECK(ipc_buffer_next_entry_size(buffer.get(), &out_size2, nullptr) == IPC_STATUS_OK);
    CHECK(out_size2 == sizeof(int));

    const int read_val = test_utils::read_data<int>(buffer.get());
    CHECK(read_val == test_data);
}

// ── single entry ──

TEST_CASE("single entry") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    const int eval = 12;
    test_utils::write_data(buffer.get(), eval);

    const int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == eval);
}

// ── fill buffer ──

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
        ipc_entry_t entry_ref = entry.get();
        const ipc_status_t read_status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);
        CHECK(read_status == IPC_STATUS_OK);
        CHECK(entry_ref.size == sizeof(size_t));

        size_t res;
        memcpy(&res, entry_ref.payload, entry_ref.size);
        CHECK(res == i);
    }

    ipc_entry_t entry_ref = entry.get();
    const ipc_status_t read_status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);
    CHECK(read_status == IPC_STATUS_EMPTY);
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

// ── wrap buffer ──

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

    // read one entry to free space
    test_utils::EntryWrapper drain_entry(sizeof(size_t));
    ipc_entry_t drain_ref = drain_entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &drain_ref, nullptr) == IPC_STATUS_OK);

    const size_t last_val = 666;
    CHECK(
        ipc_buffer_write(buffer.get(), &last_val, sizeof(last_val), nullptr) ==
        IPC_STATUS_OK);

    test_utils::EntryWrapper entry(sizeof(size_t));
    size_t prev;

    while (true) {
        ipc_entry_t entry_ref = entry.get();
        if (ipc_buffer_read(buffer.get(), &entry_ref, nullptr) != IPC_STATUS_OK) {
            break;
        }
        CHECK(entry_ref.size == sizeof(size_t));
        memcpy(&prev, entry_ref.payload, entry_ref.size);
    }

    CHECK(prev == last_val);
}

// ── integration tests ──

TEST_CASE("buffer integration - write and read sequence") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

    int data1 = 42, data2 = 84;
    test_utils::write_data(buffer.get(), data1);
    test_utils::write_data(buffer.get(), data2);

    test_utils::EntryWrapper entry(sizeof(int));

    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
    int read_val;
    memcpy(&read_val, entry_ref.payload, sizeof(int));
    CHECK(read_val == data1);

    entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
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

    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
    int read_val1;
    memcpy(&read_val1, entry_ref.payload, sizeof(int));
    CHECK(read_val1 == data1);

    entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);
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
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

        int read_val;
        memcpy(&read_val, entry_ref.payload, sizeof(int));
        CHECK(read_val == i);
    }

    for (int i = 10; i < 13; ++i) {
        test_utils::write_data(buffer.get(), i);
    }

    for (int i = 10; i < 13; ++i) {
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

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
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

    int read_val;
    memcpy(&read_val, entry_ref.payload, sizeof(int));
    CHECK(read_val == test_data);
}

TEST_CASE("buffer integration - next_size then read") {
    test_utils::BufferWrapper buffer(test_utils::MEDIUM_BUFFER_SIZE);

    int data1 = 42;
    test_utils::write_data(buffer.get(), data1);

    size_t next_size = 0;
    CHECK(ipc_buffer_next_entry_size(buffer.get(), &next_size, nullptr) == IPC_STATUS_OK);
    CHECK(next_size == sizeof(int));

    test_utils::EntryWrapper entry(next_size);
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

    int read_val;
    memcpy(&read_val, entry_ref.payload, sizeof(int));
    CHECK(read_val == data1);

    CHECK(ipc_buffer_next_entry_size(buffer.get(), &next_size, nullptr) == IPC_STATUS_EMPTY);
}

// ── boundary tests ──

TEST_CASE("buffer boundary - maximum size data") {
    test_utils::BufferWrapper buffer(test_utils::LARGE_BUFFER_SIZE);

    const size_t max_data_size = test_utils::LARGE_BUFFER_SIZE - 100;

    std::vector<uint8_t> large_data(max_data_size, 0xAB);

    const ipc_status_t write_status =
            ipc_buffer_write(buffer.get(), large_data.data(), large_data.size(), nullptr);

    if (write_status == IPC_STATUS_OK) {
        test_utils::EntryWrapper entry(max_data_size);
        ipc_entry_t entry_ref = entry.get();
        CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

        CHECK(entry_ref.size == max_data_size);

        CHECK(memcmp(entry_ref.payload, large_data.data(), max_data_size) == 0);
    }
}

TEST_CASE("buffer boundary - single byte operations") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);

    uint8_t single_byte = 0xFF;
    test_utils::write_data(buffer.get(), single_byte);

    test_utils::EntryWrapper entry(sizeof(uint8_t));
    ipc_entry_t entry_ref = entry.get();
    CHECK(ipc_buffer_read(buffer.get(), &entry_ref, nullptr) == IPC_STATUS_OK);

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
        ipc_entry_t entry_ref = entry.get();
        const ipc_status_t read_status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);

        if (read_status == IPC_STATUS_OK) {
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
        ipc_entry_t entry_ref = entry.get();
        const ipc_status_t read_status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);

        CHECK(read_status == IPC_STATUS_OK);
        CHECK(entry_ref.size == written_data[i].size());
        CHECK(memcmp(entry_ref.payload, written_data[i].data(),
            written_data[i].size()) == 0);
    }

    test_utils::EntryWrapper entry(1);
    ipc_entry_t entry_ref = entry.get();
    const ipc_status_t read_status = ipc_buffer_read(buffer.get(), &entry_ref, nullptr);
    CHECK(read_status == IPC_STATUS_EMPTY);
}
