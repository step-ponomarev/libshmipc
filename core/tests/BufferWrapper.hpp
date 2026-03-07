#pragma once

#include "shmipc/ipc_buffer.h"

class BufferWrapper {
public:
    explicit BufferWrapper(size_t size) : mem_(ipc_buffer_suggest_size(size)) {
        ipc_error_t err;
        if (ipc_buffer_init(mem_.data(), ipc_buffer_suggest_size(size), &buffer_, &err) == IPC_STATUS_ERROR) {
            throw std::runtime_error(err.message);
        }
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