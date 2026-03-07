#pragma once

#include "doctest/doctest.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_channel.h"
#include "EntryWrapper.hpp"
#include "ChannelWrapper.hpp"
#include "BufferWrapper.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace test_utils {
    constexpr size_t SMALL_BUFFER_SIZE = 256;
    constexpr size_t MEDIUM_BUFFER_SIZE = 512;
    constexpr size_t LARGE_BUFFER_SIZE = 1024;
    constexpr size_t DEFAULT_COUNT = 100000;
    constexpr size_t LARGE_COUNT = 50000;

    inline void CHECK_NULL_ARG_ERROR(ipc_status_t expected_status, ipc_error_t err) {
        CHECK(expected_status == IPC_STATUS_ERROR);
        CHECK(err.kind == IPC_ERR_KIND_ARG);
        CHECK(err.code == IPC_ERR_CODE_NULL_ARG);
        CHECK(err.message != nullptr);
    }

    inline void CHECK_INVALID_CAPACITY_ARG_ERROR(ipc_status_t expected_status, ipc_error_t err) {
        CHECK(expected_status == IPC_STATUS_ERROR);
        CHECK(err.kind == IPC_ERR_KIND_ARG);
        CHECK(err.code == IPC_ERR_CODE_INVALID_CAPACITY);
        CHECK(err.message != nullptr);
    }

    inline void CHECK_INVALID_CAPACITY_SIZE_EXCEEDS_ARG_ERROR(ipc_status_t expected_status, ipc_error_t err) {
        CHECK(expected_status == IPC_STATUS_ERROR);
        CHECK(err.kind == IPC_ERR_KIND_ARG);
        CHECK(err.code == IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER);
        CHECK(err.message != nullptr);
        CHECK(err.as.arg.size.requested_size > err.as.arg.size.limit);
        CHECK(err.as.arg.size.limit > 0);
        CHECK(err.as.arg.size.suggested_size == err.as.arg.size.limit);
    }

    inline void CHECK_CAPACITY_ERROR(ipc_status_t status,
                                     ipc_error_t err,
                                     size_t expected_provided,
                                     size_t expected_required) {
        CHECK(status == IPC_STATUS_ERROR);
        CHECK(err.kind == IPC_ERR_KIND_ARG);
        CHECK(err.code == IPC_ERR_CODE_INVALID_CAPACITY);
        CHECK(err.message != nullptr);
        CHECK(err.as.arg.capacity.provided_capacity == expected_provided);
        CHECK(err.as.arg.capacity.required_capacity == expected_required);
    }

    inline void CHECK_SIZE_ERROR(ipc_status_t expected_status,
                                 ipc_error_t err,
                                 ipc_error_code_t expected_code,
                                 uint64_t expected_requested_size,
                                 uint64_t expected_size_limit,
                                 uint64_t expected_suggested_size) {
        CHECK(expected_status == IPC_STATUS_ERROR);
        CHECK(err.kind == IPC_ERR_KIND_ARG);
        CHECK(err.code == expected_code);
        CHECK(err.message != nullptr);
        CHECK(err.as.arg.size.requested_size == expected_requested_size);
        CHECK(err.as.arg.size.limit == expected_size_limit);
        CHECK(err.as.arg.size.suggested_size == expected_suggested_size);
    }

    inline void CHECK_ERROR_NONE(ipc_status_t status, ipc_error_t err) {
        CHECK(status == IPC_STATUS_OK);
        CHECK(err.kind == IPC_ERR_KIND_NONE);
        CHECK(err.code == IPC_ERR_CODE_NONE);
        CHECK(err.message == nullptr);
    }

    template<typename T>
    void write_data(ipc_buffer_t *buffer, const T &data) {
        const ipc_status_t status =
                ipc_buffer_write(buffer, &data, sizeof(data), nullptr);
        CHECK(status == IPC_STATUS_OK);
    }

    template<typename T>
    T read_data(ipc_buffer_t *buffer) {
        EntryWrapper entry(sizeof(T));
        ipc_entry_t entry_ref = entry.get();
        const ipc_status_t status = ipc_buffer_read(buffer, &entry_ref, nullptr);
        CHECK(status == IPC_STATUS_OK);

        T data;
        memcpy(&data, entry_ref.payload, sizeof(T));
        return data;
    }

    template<typename T>
    void write_data(ipc_channel_t *channel, const T &data) {
        const ipc_status_t status =
                ipc_channel_write(channel, &data, sizeof(data), nullptr);
        CHECK(status == IPC_STATUS_OK);
    }

    template<typename T>
    T read_data(ipc_channel_t *channel, uint64_t timeout_ns) {
        ipc_entry_t entry;
        const ipc_status_t status =
                ipc_channel_read(channel, &entry, timeout_ns, nullptr);
        CHECK(status == IPC_STATUS_OK);

        T data;
        memcpy(&data, entry.payload, sizeof(T));
        free(entry.payload);
        return data;
    }

    inline void fill_buffer(ipc_buffer_t *buffer, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            write_data(buffer, i);
        }
    }

    template<typename T>
    bool write_data_safe(ipc_buffer_t *buffer, const T &data) {
        const ipc_status_t status =
                ipc_buffer_write(buffer, &data, sizeof(data), nullptr);
        return status == IPC_STATUS_OK;
    }

    template<typename T>
    bool write_data_safe(ipc_channel_t *channel, const T &data) {
        const ipc_status_t status =
                ipc_channel_write(channel, &data, sizeof(data), nullptr);
        return status == IPC_STATUS_OK;
    }

    template<typename T>
    T read_data_safe(ipc_buffer_t *buffer) {
        EntryWrapper entry(sizeof(T));
        ipc_entry_t entry_ref = entry.get();
        const ipc_status_t status = ipc_buffer_read(buffer, &entry_ref, nullptr);
        if (status != IPC_STATUS_OK) {
            throw std::runtime_error("Failed to read from buffer");
        }

        T data;
        memcpy(&data, entry_ref.payload, sizeof(T));
        return data;
    }

    template<typename T>
    T read_data_safe(ipc_channel_t *channel, uint64_t timeout_ns) {
        ipc_entry_t entry;
        const ipc_status_t status =
                ipc_channel_read(channel, &entry, timeout_ns, nullptr);
        if (status != IPC_STATUS_OK) {
            throw std::runtime_error("Failed to read from channel");
        }

        T data;
        memcpy(&data, entry.payload, sizeof(T));
        free(entry.payload);
        return data;
    }

    inline void verify_buffer_creation(ipc_buffer_t *buffer,
                                       size_t /* expected_size */) {
        if (buffer == nullptr) {
            throw std::runtime_error("Buffer is null");
        }

        const int test_value = 42;
        const ipc_status_t write_status =
                ipc_buffer_write(buffer, &test_value, sizeof(test_value), nullptr);
        CHECK(write_status == IPC_STATUS_OK);
    }

    inline void verify_channel_creation(ipc_channel_t *channel) {
        if (channel == nullptr) {
            throw std::runtime_error("Channel is null");
        }

        const int test_value = 42;
        const ipc_status_t write_status =
                ipc_channel_write(channel, &test_value, sizeof(test_value), nullptr);
        if (write_status != IPC_STATUS_OK) {
            throw std::runtime_error("Failed to write to channel");
        }
    }
}
