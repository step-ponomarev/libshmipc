#pragma once

#include <stddef.h>

class EntryWrapper {
private:
    void *payload_;
    size_t size_;

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
};

