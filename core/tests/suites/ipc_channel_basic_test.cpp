#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "../src/ipc_utils.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "test_utils.h"
#include <cstring>
#include <vector>

namespace {
    constexpr timespec DEFAULT_TIMEOUT = {0, 100000000}; // 100ms
}

TEST_CASE("channel create - null out") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, nullptr, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("channel create - null memory pointer") {
  ipc_channel_t *channel = nullptr;
  ipc_error_t err;

  const uint64_t size = ipc_channel_suggest_size(128);
  const ipc_status_t status =
      ipc_channel_create(nullptr, size, &channel, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
  CHECK(channel == nullptr);
}

TEST_CASE("channel create - too small size") {
  uint8_t mem[16];

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const uint64_t small_size = 0;

  const ipc_status_t status =
      ipc_channel_create(mem, small_size, &channel, &err);
  const uint64_t min_size = ipc_channel_get_min_size();
  test_utils::CHECK_SIZE_ERROR(
      status, err, IPC_ERR_CODE_TOO_SMALL_SIZE, small_size, min_size, min_size);
  CHECK(channel == nullptr);
}

TEST_CASE("channel create - out is zeroed on error") {
  const uint64_t size = ipc_channel_suggest_size(128);

  ipc_channel_t *out = reinterpret_cast<ipc_channel_t *>(0xBAD); // any non-null to verify *out is cleared on error
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(nullptr, size, &out, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
  CHECK(out == nullptr);
}

TEST_CASE("channel create - error reset after failed then success") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;

  ipc_status_t status =
      ipc_channel_create(nullptr, size, &channel, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
  CHECK(channel == nullptr);

  status = ipc_channel_create(mem.data(), size, &channel, &err);
  test_utils::CHECK_ERROR_NONE(status, err);
  CHECK(channel != nullptr);

  ipc_channel_destroy(channel);
}

TEST_CASE("channel create - success case") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  ipc_channel_destroy(channel);
}

TEST_CASE("channel connect - null out") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_connect(mem.data(), nullptr, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
}

TEST_CASE("channel connect - null memory") {
  ipc_channel_t *channel = nullptr;
  ipc_error_t err;

  const ipc_status_t status =
      ipc_channel_connect(nullptr, &channel, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
  CHECK(channel == nullptr);
}

TEST_CASE("channel connect - out is null on error") {
  ipc_channel_t *out = reinterpret_cast<ipc_channel_t *>(0xBAD);
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_connect(nullptr, &out, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
  CHECK(out == nullptr);
}

TEST_CASE("channel connect - error reset after failed then success") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *created = nullptr;
  CHECK(ipc_channel_create(mem.data(), size, &created, nullptr) == IPC_STATUS_OK);

  ipc_channel_t *connected = nullptr;
  ipc_error_t err;
  ipc_status_t status = ipc_channel_connect(nullptr, &connected, &err);
  test_utils::CHECK_NULL_ARG_ERROR(status, err);
  CHECK(connected == nullptr);

  status = ipc_channel_connect(mem.data(), &connected, &err);
  test_utils::CHECK_ERROR_NONE(status, err);
  CHECK(connected != nullptr);

  ipc_channel_destroy(created);
  ipc_channel_destroy(connected);
}

TEST_CASE("channel connect - success case") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *created = nullptr;
  const ipc_status_t create_status =
      ipc_channel_create(mem.data(), size, &created, nullptr);
  CHECK(create_status == IPC_STATUS_OK);

  const int test_value = 42;
  CHECK(ipc_channel_write(created, &test_value, sizeof(test_value), nullptr) == IPC_STATUS_OK);

  ipc_channel_t *connected = nullptr;
  const ipc_status_t connect_status =
      ipc_channel_connect(mem.data(), &connected, nullptr);
  CHECK(connect_status == IPC_STATUS_OK);

  const int res = test_utils::read_data_safe<int>(connected, &DEFAULT_TIMEOUT);
  CHECK(res == test_value);

  ipc_channel_destroy(created);
  ipc_channel_destroy(connected);
}

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
      ipc_channel_create(mem.data(), size, &channel, &err);
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
  ipc_channel_destroy(channel);
}

TEST_CASE("write read") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *producer = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &producer, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(producer != nullptr);

  const int val = 43;
  CHECK(ipc_channel_write(producer, &val, sizeof(val), nullptr) == IPC_STATUS_OK);

  ipc_channel_t *consumer = nullptr;
  ipc_error_t connect_err;
  const ipc_status_t connect_status =
      ipc_channel_connect(mem.data(), &consumer, &connect_err);
  CHECK(connect_status == IPC_STATUS_OK);
  CHECK(consumer != nullptr);

  const int res = test_utils::read_data_safe<int>(consumer, &DEFAULT_TIMEOUT);
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

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  const int expected = 42;
  CHECK(ipc_channel_write(channel, &expected, sizeof(expected), nullptr) == IPC_STATUS_OK);

  ipc_entry_t entry;
  IpcChannelPeekResult pk = ipc_channel_peek(channel, &entry);
  CHECK(pk.ipc_status == IPC_OK);
  CHECK(entry.size == sizeof(expected));

  int peeked;
  memcpy(&peeked, entry.payload, sizeof(expected));
  CHECK(peeked == expected);

  ipc_entry_t entry2;
  IpcChannelReadResult rd =
      ipc_channel_read(channel, &entry2, &DEFAULT_TIMEOUT);
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

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  ipc_entry_t entry;
  IpcChannelPeekResult pk = ipc_channel_peek(channel, &entry);
  CHECK(pk.ipc_status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}

TEST_CASE("write try read") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  const int expected = 42;
  CHECK(ipc_channel_write(channel, &expected, sizeof(expected), nullptr) == IPC_STATUS_OK);

  ipc_entry_t entry;
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

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  ipc_entry_t entry;
  CHECK(ipc_channel_try_read(channel, &entry).ipc_status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}

TEST_CASE("read retry limit reached") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  const int expected = -11;
  CHECK(ipc_channel_write(channel, &expected, sizeof(expected), nullptr) == IPC_STATUS_OK);

  ipc_entry_t peek_entry;
  IpcChannelPeekResult pk = ipc_channel_peek(channel, &peek_entry);
  CHECK(IpcChannelPeekResult_is_ok(pk));

  uint64_t *seq_ptr =
      (uint64_t *)((uint8_t *)peek_entry.payload - sizeof(uint64_t) * 3);
  uint64_t original_seq = *seq_ptr;
  *seq_ptr = 0xDEADBEEF;

  ipc_entry_t entry;
  CHECK(ipc_channel_read(channel, &entry, &DEFAULT_TIMEOUT).ipc_status ==
        IPC_ERR_TIMEOUT);

  *seq_ptr = original_seq;

  CHECK(ipc_channel_read(channel, &entry, &DEFAULT_TIMEOUT).ipc_status ==
        IPC_OK);

  int res;
  memcpy(&res, entry.payload, sizeof(expected));
  CHECK(expected == res);
  free(entry.payload);

  ipc_channel_destroy(channel);
}

TEST_CASE("skip corrupted entry") {
  const uint64_t size = ipc_channel_suggest_size(256);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  const int first_val = 100;
  const int second_val = -11;

  CHECK(ipc_channel_write(channel, &first_val, sizeof(first_val), nullptr) == IPC_STATUS_OK);
  CHECK(ipc_channel_write(channel, &second_val, sizeof(second_val), nullptr) == IPC_STATUS_OK);

  ipc_entry_t peek_entry;
  IpcChannelPeekResult pk = ipc_channel_peek(channel, &peek_entry);
  CHECK(IpcChannelPeekResult_is_ok(pk));

  if (peek_entry.payload != nullptr && peek_entry.size >= sizeof(int)) {
    uint8_t *corrupt_ptr = (uint8_t *)peek_entry.payload - sizeof(uint64_t) * 3;
    *((uint64_t *)corrupt_ptr) = 0xDEADBEEF;
  }

  ipc_entry_t entry;
  IpcChannelReadResult read_res =
      ipc_channel_read(channel, &entry, &DEFAULT_TIMEOUT);
  CHECK(read_res.ipc_status == IPC_ERR_TIMEOUT);

  pk = ipc_channel_peek(channel, &entry);
  CHECK(IpcChannelPeekResult_is_error(pk));

  CHECK(ipc_channel_skip_force(channel).ipc_status == IPC_OK);

  read_res = ipc_channel_read(channel, &entry, &DEFAULT_TIMEOUT);
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

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  const int val = 42;
  CHECK(ipc_channel_write(channel, &val, sizeof(val), nullptr) == IPC_STATUS_OK);

  ipc_entry_t entry;
  CHECK(ipc_channel_peek(channel, &entry).ipc_status == IPC_OK);
  CHECK(ipc_channel_skip_force(channel).ipc_status == IPC_OK);
  CHECK(ipc_channel_peek(channel, &entry).ipc_status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}

TEST_CASE("read timeout") {
  const uint64_t size = ipc_channel_suggest_size(128);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
  const uint64_t timeout_ns = ipc_timespec_to_nanos(&timeout);

  struct timespec time;
  CHECK(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
  const uint64_t before_ns = ipc_timespec_to_nanos(&time);

  ipc_entry_t entry;
  CHECK(ipc_channel_read(channel, &entry, &timeout).ipc_status ==
        IPC_ERR_TIMEOUT);

  CHECK(clock_gettime(CLOCK_MONOTONIC, &time) == 0);

  const uint64_t after_ns = ipc_timespec_to_nanos(&time);
  CHECK(after_ns - before_ns >= timeout_ns);

  ipc_channel_destroy(channel);
}

TEST_CASE("channel data - different sizes") {
  struct timespec timeout = {0, 100000000}; // 100ms
  const uint64_t size = ipc_channel_suggest_size(2048);
  std::vector<uint8_t> mem(size);

  ipc_channel_t *channel = nullptr;
  ipc_error_t err;
  const ipc_status_t status =
      ipc_channel_create(mem.data(), size, &channel, &err);
  CHECK(status == IPC_STATUS_OK);
  CHECK(channel != nullptr);

  struct TestData {
    size_t size;
    uint8_t pattern;
  };

  std::vector<TestData> test_cases = {{1, 0xAA},   {4, 0xBB},  {8, 0xCC},
                                      {16, 0xDD},  {32, 0xEE}, {64, 0xFF},
                                      {128, 0x11}, {256, 0x22}};

  std::vector<std::vector<uint8_t>> written_data;
  for (const auto &test_case : test_cases) {
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
    IpcChannelReadResult read_result =
        ipc_channel_read(channel, &entry, &timeout);

    CHECK(read_result.ipc_status == IPC_OK);
    CHECK(entry.size == written_data[i].size());
    CHECK(memcmp(entry.payload, written_data[i].data(),
                 written_data[i].size()) == 0);

    free(entry.payload);
  }

  ipc_entry_t entry;
  IpcChannelTryReadResult read_result = ipc_channel_try_read(channel, &entry);
  CHECK(read_result.ipc_status == IPC_EMPTY);

  ipc_channel_destroy(channel);
}
