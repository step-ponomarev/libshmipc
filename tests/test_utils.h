#pragma once

#include "doctest/doctest.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

template<typename T>
class concurrent_set;

namespace test_utils {
constexpr size_t SMALL_BUFFER_SIZE = 128;
constexpr size_t MEDIUM_BUFFER_SIZE = 256;
constexpr size_t LARGE_BUFFER_SIZE = 1024;
constexpr size_t DEFAULT_COUNT = 200000;
constexpr size_t LARGE_COUNT = 300000;

const IpcChannelConfiguration DEFAULT_CONFIG = {
    .max_round_trips = 1024, 
    .start_sleep_ns = 1000, 
    .max_sleep_ns = 100000
};
class BufferWrapper {
public:
    explicit BufferWrapper(size_t size) : mem_(size) {
        const IpcBufferCreateResult result = ipc_buffer_create(mem_.data(), size);
        CHECK(IpcBufferCreateResult_is_ok(result));
        buffer_ = result.result;
    }
    
    ~BufferWrapper() {
        if (buffer_) {
            free(buffer_);
        }
    }
    
    IpcBuffer* get() const { return buffer_; }
    IpcBuffer* operator->() const { return buffer_; }
    
    BufferWrapper(const BufferWrapper&) = delete;
    BufferWrapper& operator=(const BufferWrapper&) = delete;
    BufferWrapper(BufferWrapper&& other) noexcept 
        : buffer_(other.buffer_), mem_(std::move(other.mem_)) {
        other.buffer_ = nullptr;
    }
    
    BufferWrapper& operator=(BufferWrapper&& other) noexcept {
        if (this != &other) {
            if (buffer_) free(buffer_);
            buffer_ = other.buffer_;
            mem_ = std::move(other.mem_);
            other.buffer_ = nullptr;
        }
        return *this;
    }

private:
    IpcBuffer* buffer_;
    std::vector<uint8_t> mem_;
};

class ChannelWrapper {
public:
    explicit ChannelWrapper(size_t size, const IpcChannelConfiguration& config = DEFAULT_CONFIG) 
        : mem_(size) {
        const IpcChannelResult result = ipc_channel_create(mem_.data(), size, config);
        CHECK(IpcChannelResult_is_ok(result));
        channel_ = result.result;
    }
    
    ~ChannelWrapper() {
        if (channel_) {
            ipc_channel_destroy(channel_);
        }
    }
    
    IpcChannel* get() const { return channel_; }
    IpcChannel* operator->() const { return channel_; }
    
    ChannelWrapper(const ChannelWrapper&) = delete;
    ChannelWrapper& operator=(const ChannelWrapper&) = delete;
    ChannelWrapper(ChannelWrapper&& other) noexcept 
        : channel_(other.channel_), mem_(std::move(other.mem_)) {
        other.channel_ = nullptr;
    }
    
    ChannelWrapper& operator=(ChannelWrapper&& other) noexcept {
        if (this != &other) {
            if (channel_) ipc_channel_destroy(channel_);
            channel_ = other.channel_;
            mem_ = std::move(other.mem_);
            other.channel_ = nullptr;
        }
        return *this;
    }

private:
    IpcChannel* channel_;
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
    
    IpcEntry get() const {
        return {.payload = payload_, .size = size_};
    }
    
    void* payload() const { return payload_; }
    size_t size() const { return size_; }
    
    EntryWrapper(const EntryWrapper&) = delete;
    EntryWrapper& operator=(const EntryWrapper&) = delete;
    EntryWrapper(EntryWrapper&& other) noexcept 
        : payload_(other.payload_), size_(other.size_) {
        other.payload_ = nullptr;
        other.size_ = 0;
    }
    
    EntryWrapper& operator=(EntryWrapper&& other) noexcept {
        if (this != &other) {
            if (payload_) free(payload_);
            payload_ = other.payload_;
            size_ = other.size_;
            other.payload_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

private:
    void* payload_;
    size_t size_;
};

void CHECK_OK(const IpcBufferCreateResult& result) {
    CHECK(IpcBufferCreateResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferCreateResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferCreateResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferCreateResult& result, IpcStatus expected_status, 
                            size_t expected_requested_size, size_t expected_min_size) {
    CHECK(IpcBufferCreateResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.requested_size == expected_requested_size);
    CHECK(result.error.body.min_size == expected_min_size);
}

void CHECK_OK(const IpcBufferAttachResult& result) {
    CHECK(IpcBufferAttachResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferAttachResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferAttachResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferAttachResult& result, IpcStatus expected_status, 
                            size_t expected_min_size) {
    CHECK(IpcBufferAttachResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.min_size == expected_min_size);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferWriteResult& result, IpcStatus expected_status,
                            uint64_t expected_offset, size_t expected_requested_size,
                            size_t expected_available_contiguous, size_t expected_buffer_size) {
    CHECK(IpcBufferWriteResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.offset == expected_offset);
    CHECK(result.error.body.requested_size == expected_requested_size);
    CHECK(result.error.body.available_contiguous == expected_available_contiguous);
    CHECK(result.error.body.buffer_size == expected_buffer_size);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferReadResult& result, IpcStatus expected_status,
                            uint64_t expected_offset, size_t expected_required_size) {
    CHECK(IpcBufferReadResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.offset == expected_offset);
    CHECK(result.error.body.required_size == expected_required_size);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferPeekResult& result, IpcStatus expected_status,
                            uint64_t expected_offset) {
    CHECK(IpcBufferPeekResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.offset == expected_offset);
}

void CHECK_OK(const IpcBufferSkipResult& result) {
    CHECK(IpcBufferSkipResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferSkipResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferSkipResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferSkipResult& result, IpcStatus expected_status,
                            uint64_t expected_offset) {
    CHECK(IpcBufferSkipResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.offset == expected_offset);
}

void CHECK_OK(const IpcBufferSkipForceResult& result) {
    CHECK(IpcBufferSkipForceResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferSkipForceResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferSkipForceResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferSkipForceResult& result, IpcStatus expected_status,
                              bool expected_unit) {
    CHECK(IpcBufferSkipForceResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body._unit == expected_unit);
}

// IpcBufferReserveEntryResult utilities
void CHECK_OK(const IpcBufferReserveEntryResult& result) {
    CHECK(IpcBufferReserveEntryResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferReserveEntryResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferReserveEntryResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferReserveEntryResult& result, IpcStatus expected_status,
                              uint64_t expected_offset, uint64_t expected_buffer_size,
                              size_t expected_required_size, size_t expected_free_space) {
    CHECK(IpcBufferReserveEntryResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.offset == expected_offset);
    CHECK(result.error.body.buffer_size == expected_buffer_size);
    CHECK(result.error.body.required_size == expected_required_size);
    CHECK(result.error.body.free_space == expected_free_space);
}

// IpcBufferCommitEntryResult utilities
void CHECK_OK(const IpcBufferCommitEntryResult& result) {
    CHECK(IpcBufferCommitEntryResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferCommitEntryResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferCommitEntryResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_ERROR_WITH_FIELDS(const IpcBufferCommitEntryResult& result, IpcStatus expected_status,
                              uint64_t expected_offset) {
    CHECK(IpcBufferCommitEntryResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
    CHECK(result.error.body.offset == expected_offset);
}

void CHECK_OK(const IpcBufferWriteResult& result) {
    CHECK(IpcBufferWriteResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferWriteResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferWriteResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcBufferReadResult& result) {
    CHECK(IpcBufferReadResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferReadResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferReadResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcBufferPeekResult& result) {
    CHECK(IpcBufferPeekResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferPeekResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferPeekResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcChannelResult& result) {
    CHECK(IpcChannelResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcChannelWriteResult& result) {
    CHECK(IpcChannelWriteResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelWriteResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelWriteResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcChannelReadResult& result) {
    CHECK(IpcChannelReadResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelReadResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelReadResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcChannelPeekResult& result) {
    CHECK(IpcChannelPeekResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelPeekResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelPeekResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

template<typename T>
void write_data(IpcBuffer* buffer, const T& data) {
    const IpcBufferWriteResult result = ipc_buffer_write(buffer, &data, sizeof(data));
    CHECK_OK(result);
}

template<typename T>
T read_data(IpcBuffer* buffer) {
    EntryWrapper entry(sizeof(T));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
    CHECK_OK(result);
    
    T data;
    memcpy(&data, entry_ref.payload, sizeof(T));
    return data;
}

template<typename T>
void write_data(IpcChannel* channel, const T& data) {
    const IpcChannelWriteResult result = ipc_channel_write(channel, &data, sizeof(data));
    CHECK(IpcChannelWriteResult_is_ok(result));
}

template<typename T>
T read_data(IpcChannel* channel) {
    IpcEntry entry;
    const IpcChannelReadResult result = ipc_channel_read(channel, &entry);
    CHECK(IpcChannelReadResult_is_ok(result));
    
    T data;
    memcpy(&data, entry.payload, sizeof(T));
    free(entry.payload);
    return data;
}

template<typename T>
T peek_data(IpcBuffer* buffer) {
    IpcEntry entry;
    const IpcBufferPeekResult result = ipc_buffer_peek(buffer, &entry);
    CHECK_OK(result);
    
    T data;
    memcpy(&data, entry.payload, sizeof(T));
    return data;
}

template<typename T>
T peek_data(IpcChannel* channel) {
    IpcEntry entry;
    const IpcChannelPeekResult result = ipc_channel_peek(channel, &entry);
    CHECK(IpcChannelPeekResult_is_ok(result));
    
    T data;
    memcpy(&data, entry.payload, sizeof(T));
    return data;
}

template<typename T>
void reserve_and_write(IpcBuffer* buffer, const T& data) {
    void* dest;
    const IpcBufferReserveEntryResult result = ipc_buffer_reserve_entry(buffer, sizeof(T), &dest);
    CHECK(IpcBufferReserveEntryResult_is_ok(result));
    
    memcpy(dest, &data, sizeof(T));
    const IpcBufferCommitEntryResult commit_result = ipc_buffer_commit_entry(buffer, result.result);
    CHECK(IpcBufferCommitEntryResult_is_ok(commit_result));
}

void fill_buffer(IpcBuffer* buffer, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        write_data(buffer, i);
    }
}

} // namespace test_utils
