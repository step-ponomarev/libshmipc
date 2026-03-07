#pragma once

class ChannelWrapper {
private:
    ipc_channel_t *channel_;
    std::vector<uint8_t> mem_;

public:
    explicit ChannelWrapper(size_t size) : mem_(ipc_channel_suggest_size(size)) {
        ipc_error_t err;
        if (ipc_channel_init(mem_.data(),
                                   ipc_channel_suggest_size(size),
                                   &channel_,
                                   &err) == IPC_STATUS_ERROR) {
            throw std::runtime_error(err.message);
                                   }
    }

    ~ChannelWrapper() {
        if (channel_) {
            ipc_channel_detach(channel_, nullptr);
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
                ipc_channel_detach(channel_, nullptr);
            }
            channel_ = other.channel_;
            mem_ = std::move(other.mem_);
            other.channel_ = nullptr;
        }
        return *this;
    }
};

