#pragma once

#include "doctest/doctest.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
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

const IpcChannelConfiguration DEFAULT_CONFIG = {
    .max_round_trips = 1024,
    .start_sleep_ns = 1000,
    .max_sleep_ns = 100000
};
class BufferWrapper {
public:
    explicit BufferWrapper(size_t size) : mem_(ipc_buffer_align_size(size)) {
        const IpcBufferCreateResult result = ipc_buffer_create(mem_.data(), ipc_buffer_align_size(size));
        CHECK(IpcBufferCreateResult_is_ok(result));
        buffer_ = result.result;
    }
    
    ~BufferWrapper() {
        // IpcBuffer doesn't require explicit cleanup
        // The memory is managed by the mem_ vector
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
            // IpcBuffer doesn't require explicit cleanup
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
        : mem_(ipc_channel_align_size(size)) {
        const IpcChannelResult result = ipc_channel_create(mem_.data(), ipc_channel_align_size(size), config);
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
    const uint8_t* get_mem() const { return mem_.data(); }
    
    // Release ownership of the channel (caller becomes responsible for destroying it)
    IpcChannel* release() {
        IpcChannel* result = channel_;
        channel_ = nullptr;
        return result;
    }
    
    ChannelWrapper(const ChannelWrapper&) = delete;
    ChannelWrapper& operator=(const ChannelWrapper&) = delete;
    ChannelWrapper(ChannelWrapper&& other) noexcept 
        : channel_(other.channel_), mem_(std::move(other.mem_)) {
        other.channel_ = nullptr;
    }
    
    ChannelWrapper& operator=(ChannelWrapper&& other) noexcept {
        if (this != &other) {
            if (channel_) {
                ipc_channel_destroy(channel_);
            }
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
        return {.offset = 0, .payload = payload_, .size = size_};
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

void CHECK_OK(const IpcBufferAttachResult& result) {
    CHECK(IpcBufferAttachResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferAttachResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferAttachResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcBufferSkipResult& result) {
    CHECK(IpcBufferSkipResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferSkipResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferSkipResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcBufferSkipForceResult& result) {
    CHECK(IpcBufferSkipForceResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferSkipForceResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferSkipForceResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcBufferWriteResult& result) {
    CHECK(IpcBufferWriteResult_is_ok(result));
}

void CHECK_ERROR(const IpcBufferWriteResult& result, IpcStatus expected_status) {
    CHECK(IpcBufferWriteResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcBufferReadResult& result) {
    CHECK(result.ipc_status == IPC_OK);
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

void CHECK_OK(const IpcChannelConnectResult& result) {
    CHECK(IpcChannelConnectResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelConnectResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelConnectResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcChannelDestroyResult& result) {
    CHECK(IpcChannelDestroyResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelDestroyResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelDestroyResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}


void CHECK_OK(const IpcChannelReadWithTimeoutResult& result) {
    CHECK(IpcChannelReadWithTimeoutResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelReadWithTimeoutResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelReadWithTimeoutResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}


void CHECK_OK(const IpcChannelSkipForceResult& result) {
    CHECK(IpcChannelSkipForceResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelSkipForceResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelSkipForceResult_is_error(result));
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
    CHECK(result.ipc_status == IPC_OK);
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

void CHECK_OK(const IpcChannelTryReadResult& result) {
    CHECK(IpcChannelTryReadResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelTryReadResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelTryReadResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}

void CHECK_OK(const IpcChannelSkipResult& result) {
    CHECK(IpcChannelSkipResult_is_ok(result));
}

void CHECK_ERROR(const IpcChannelSkipResult& result, IpcStatus expected_status) {
    CHECK(IpcChannelSkipResult_is_error(result));
    CHECK(result.ipc_status == expected_status);
}


template<typename T>
void write_data(IpcBuffer* buffer, const T& data) {
    const IpcBufferWriteResult result = ipc_buffer_write(buffer, &data, sizeof(data));
    CHECK(result.ipc_status == IPC_OK);
}

template<typename T>
T read_data(IpcBuffer* buffer) {
    EntryWrapper entry(sizeof(T));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
    CHECK(result.ipc_status == IPC_OK);
    
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
    CHECK(result.ipc_status == IPC_OK);
    
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

void fill_buffer(IpcBuffer* buffer, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        write_data(buffer, i);
    }
}

template<typename T>
bool write_data_safe(IpcBuffer* buffer, const T& data) {
    const IpcBufferWriteResult result = ipc_buffer_write(buffer, &data, sizeof(data));
    return result.ipc_status == IPC_OK;
}

template<typename T>
bool write_data_safe(IpcChannel* channel, const T& data) {
    const IpcChannelWriteResult result = ipc_channel_write(channel, &data, sizeof(data));
    return result.ipc_status == IPC_OK;
}

template<typename T>
T read_data_safe(IpcBuffer* buffer) {
    test_utils::EntryWrapper entry(sizeof(T));
    IpcEntry entry_ref = entry.get();
    const IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
    if (result.ipc_status != IPC_OK) {
        throw std::runtime_error("Failed to read from buffer");
    }
    
    T data;
    memcpy(&data, entry_ref.payload, sizeof(T));
    return data;
}

template<typename T>
T read_data_safe(IpcChannel* channel) {
    IpcEntry entry;
    const IpcChannelReadResult result = ipc_channel_read(channel, &entry);
    if (result.ipc_status != IPC_OK) {
        throw std::runtime_error("Failed to read from channel");
    }
    
    T data;
    memcpy(&data, entry.payload, sizeof(T));
    free(entry.payload);
    return data;
}

void verify_buffer_creation(IpcBuffer* buffer, size_t /* expected_size */) {
    if (buffer == nullptr) {
        throw std::runtime_error("Buffer is null");
    }
    
    const int test_value = 42;
    const IpcBufferWriteResult write_result = ipc_buffer_write(buffer, &test_value, sizeof(test_value));
    if (write_result.ipc_status != IPC_OK) {
        throw std::runtime_error("Failed to write to buffer");
    }
}

void verify_channel_creation(IpcChannel* channel) {
    if (channel == nullptr) {
        throw std::runtime_error("Channel is null");
    }
    
    const int test_value = 42;
    const IpcChannelWriteResult write_result = ipc_channel_write(channel, &test_value, sizeof(test_value));
    if (write_result.ipc_status != IPC_OK) {
        throw std::runtime_error("Failed to write to channel");
    }
}

} 
