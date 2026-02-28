#pragma once

#include "doctest/doctest.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_channel.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

template<typename T>
class ConcurrentSet;

namespace test_utils {
    constexpr size_t SMALL_BUFFER_SIZE = 256;
    constexpr size_t MEDIUM_BUFFER_SIZE = 512;
    constexpr size_t LARGE_BUFFER_SIZE = 1024;
    constexpr size_t DEFAULT_COUNT = 100000;
    constexpr size_t LARGE_COUNT = 50000;

    class BufferWrapper {
    public:
        explicit BufferWrapper(size_t size) : mem_(ipc_buffer_suggest_size(size)) {
            const ipc_status_t status = ipc_buffer_init(mem_.data(), ipc_buffer_suggest_size(size), &buffer_, NULL);
            CHECK(status == IPC_STATUS_OK);
        }

        ~BufferWrapper() {
            if (buffer_) {
                ipc_buffer_detach(buffer_, nullptr);
            }
        }

        ipc_buffer_t *get() const { return buffer_; }
        ipc_buffer_t *operator->() const { return buffer_; }

        BufferWrapper(const BufferWrapper &) = delete;

        BufferWrapper &operator=(const BufferWrapper &) = delete;

        BufferWrapper(BufferWrapper &&other) noexcept
            : buffer_(other.buffer_), mem_(std::move(other.mem_)) {
            other.buffer_ = nullptr;
        }

        BufferWrapper &operator=(BufferWrapper &&other) noexcept {
            if (this != &other) {
                buffer_ = other.buffer_;
                mem_ = std::move(other.mem_);
                other.buffer_ = nullptr;
            }
            return *this;
        }

    private:
        ipc_buffer_t *buffer_;
        std::vector<uint8_t> mem_;
    };

    class ChannelWrapper {
    public:
        explicit ChannelWrapper(size_t size) : mem_(ipc_channel_suggest_size(size)) {
            channel_ = nullptr;
            ipc_error_t err;
            const ipc_status_t status =
                    ipc_channel_init(mem_.data(),
                                       ipc_channel_suggest_size(size),
                                       &channel_,
                                       &err);
            CHECK(status == IPC_STATUS_OK);
            CHECK(channel_ != nullptr);
        }

        ~ChannelWrapper() {
            if (channel_) {
                ipc_channel_detach(channel_);
            }
        }

        ipc_channel_t *get() const { return channel_; }
        ipc_channel_t *operator->() const { return channel_; }
        const uint8_t *get_mem() const { return mem_.data(); }

        ipc_channel_t *release() {
            ipc_channel_t *result = channel_;
            channel_ = nullptr;
            return result;
        }

        ChannelWrapper(const ChannelWrapper &) = delete;

        ChannelWrapper &operator=(const ChannelWrapper &) = delete;

        ChannelWrapper(ChannelWrapper &&other) noexcept
            : channel_(other.channel_), mem_(std::move(other.mem_)) {
            other.channel_ = nullptr;
        }

        ChannelWrapper &operator=(ChannelWrapper &&other) noexcept {
            if (this != &other) {
                if (channel_) {
                    ipc_channel_detach(channel_);
                }
                channel_ = other.channel_;
                mem_ = std::move(other.mem_);
                other.channel_ = nullptr;
            }
            return *this;
        }

    private:
        ipc_channel_t *channel_;
        std::vector<uint8_t> mem_;
    };

    class EntryWrapper {
    public:
        explicit EntryWrapper(size_t size) : size_(size) {
            payload_ = malloc(size);
            CHECK(payload_ != nullptr);
        }

        ~EntryWrapper() {
            if (payload_) {
                free(payload_);
            }
        }

        ipc_entry_t get() const {
            return {.offset = 0, .payload = payload_, .size = size_};
        }

        void *payload() const { return payload_; }
        size_t size() const { return size_; }

        EntryWrapper(const EntryWrapper &) = delete;

        EntryWrapper &operator=(const EntryWrapper &) = delete;

        EntryWrapper(EntryWrapper &&other) noexcept
            : payload_(other.payload_), size_(other.size_) {
            other.payload_ = nullptr;
            other.size_ = 0;
        }

        EntryWrapper &operator=(EntryWrapper &&other) noexcept {
            if (this != &other) {
                if (payload_) {
                    free(payload_);
                }
                payload_ = other.payload_;
                size_ = other.size_;
                other.payload_ = nullptr;
                other.size_ = 0;
            }
            return *this;
        }

    private:
        void *payload_;
        size_t size_;
    };

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

    inline void CHECK_OK(const IpcChannelDestroyResult &result) {
        CHECK(IpcChannelDestroyResult_is_ok(result));
    }

    inline void CHECK_ERROR(const IpcChannelDestroyResult &result,
                            IpcStatus expected_status) {
        CHECK(IpcChannelDestroyResult_is_error(result));
        CHECK(result.ipc_status == expected_status);
    }

    inline void CHECK_OK(const IpcChannelSkipForceResult &result) {
        CHECK(IpcChannelSkipForceResult_is_ok(result));
    }

    inline void CHECK_ERROR(const IpcChannelSkipForceResult &result,
                            IpcStatus expected_status) {
        CHECK(IpcChannelSkipForceResult_is_error(result));
        CHECK(result.ipc_status == expected_status);
    }

    inline void CHECK_OK(const IpcChannelReadResult &result) {
        CHECK(result.ipc_status == IPC_OK);
    }

    inline void CHECK_ERROR(const IpcChannelReadResult &result,
                            IpcStatus expected_status) {
        CHECK(IpcChannelReadResult_is_error(result));
        CHECK(result.ipc_status == expected_status);
    }

    inline void CHECK_OK(const IpcChannelPeekResult &result) {
        CHECK(IpcChannelPeekResult_is_ok(result));
    }

    inline void CHECK_ERROR(const IpcChannelPeekResult &result,
                            IpcStatus expected_status) {
        CHECK(IpcChannelPeekResult_is_error(result));
        CHECK(result.ipc_status == expected_status);
    }

    inline void CHECK_OK(const IpcChannelTryReadResult &result) {
        CHECK(IpcChannelTryReadResult_is_ok(result));
    }

    inline void CHECK_ERROR(const IpcChannelTryReadResult &result,
                            IpcStatus expected_status) {
        CHECK(IpcChannelTryReadResult_is_error(result));
        CHECK(result.ipc_status == expected_status);
    }

    inline void CHECK_OK(const IpcChannelSkipResult &result) {
        CHECK(IpcChannelSkipResult_is_ok(result));
    }

    inline void CHECK_ERROR(const IpcChannelSkipResult &result,
                            IpcStatus expected_status) {
        CHECK(IpcChannelSkipResult_is_error(result));
        CHECK(result.ipc_status == expected_status);
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
    T read_data(ipc_channel_t *channel, const struct timespec *timeout) {
        ipc_entry_t entry;
        const IpcChannelReadResult result =
                ipc_channel_read(channel, &entry, timeout);
        CHECK(result.ipc_status == IPC_OK);

        T data;
        memcpy(&data, entry.payload, sizeof(T));
        free(entry.payload);
        return data;
    }

    template<typename T>
    T peek_data(ipc_channel_t *channel) {
        ipc_entry_t entry;
        const IpcChannelPeekResult result = ipc_channel_peek(channel, &entry);
        CHECK(IpcChannelPeekResult_is_ok(result));

        T data;
        memcpy(&data, entry.payload, sizeof(T));
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
        test_utils::EntryWrapper entry(sizeof(T));
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
    T read_data_safe(ipc_channel_t *channel, const struct timespec *timeout) {
        ipc_entry_t entry;
        const IpcChannelReadResult result =
                ipc_channel_read(channel, &entry, timeout);
        if (result.ipc_status != IPC_OK) {
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
} // namespace test_utils
