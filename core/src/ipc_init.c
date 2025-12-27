#include "ipc_utils.h"
#include <shmipc/ipc_init.h>
#include <shmipc/ipc_mmap.h>
#include <stdint.h>
#include <unistd.h>

#define NOT_ENOUGH_SIZE_ERROR_MESSAGE                                          \
  "Not enough size for initialization, see recommended min size"
#define NOT_ALIGNED_ERROR_MESSAGE                                              \
  "(Size - overhead) must be power of 2, see recommended min size"

typedef struct {
  IpcStatus status;
  const char *error_msg;
  uint64_t overhead;
  uint64_t min_size;
} ValidationResult;

static uint64_t _find_next_power_of_2(uint64_t n);

static ValidationResult _validate_size(size_t requested_size, uint64_t min_size,
                                       uint64_t overhead);

uint64_t ipc_init_suggest_buffer_size(size_t desired_capacity) {
  const uint64_t min_size = ipc_buffer_get_min_size();
  const uint64_t overhead = ipc_buffer_get_memory_overhead();

  if (desired_capacity + overhead < min_size) {
    return min_size;
  }

  const uint64_t aligned_capacity = _find_next_power_of_2(desired_capacity);
  return aligned_capacity + overhead;
}

uint64_t ipc_init_suggest_channel_size(size_t desired_capacity) {
  const uint64_t min_size = ipc_channel_get_min_size();
  const uint64_t overhead = ipc_channel_get_memory_overhead();

  if (desired_capacity + overhead < min_size) {
    return min_size;
  }

  const uint64_t aligned_capacity = _find_next_power_of_2(desired_capacity);
  return aligned_capacity + overhead;
}

IpcInitBufferCreateResult ipc_init_buffer_create(const char *path,
                                                 const size_t size) {
  IpcInitBufferCreateError error = {.requested_size = size};

  const ValidationResult validation = _validate_size(
      size, ipc_buffer_get_min_size(), ipc_buffer_get_memory_overhead());

  if (validation.status != IPC_OK) {
    error.min_size = validation.min_size;
    error.overhead = validation.overhead;
    return IpcInitBufferCreateResult_error_body(validation.status,
                                                validation.error_msg, error);
  }

  const IpcMemorySegmentResult mmap = ipc_mmap(path, size);
  if (IpcMemorySegmentResult_is_error(mmap)) {
    error.sys_errno = mmap.error.body.sys_errno;
    return IpcInitBufferCreateResult_error_body(mmap.ipc_status,
                                                mmap.error.detail, error);
  }

  const IpcBufferCreateResult buffer_result =
      ipc_buffer_create(mmap.result, size);
  if (IpcBufferCreateResult_is_error(buffer_result)) {
    error.sys_errno = buffer_result.error.body.sys_errno;
    return IpcInitBufferCreateResult_error_body(
        buffer_result.ipc_status, buffer_result.error.detail, error);
  }

  return IpcInitBufferCreateResult_ok(IPC_OK, buffer_result.result);
}

IpcInitBufferAttachResult ipc_init_buffer_attach(const char *path,
                                                 const size_t size) {
  IpcInitBufferAttachError error = {.requested_size = size};

  const ValidationResult validation = _validate_size(
      size, ipc_buffer_get_min_size(), ipc_buffer_get_memory_overhead());

  if (validation.status != IPC_OK) {
    error.min_size = validation.min_size;
    error.overhead = validation.overhead;
    return IpcInitBufferAttachResult_error_body(validation.status,
                                                validation.error_msg, error);
  }

  const IpcMemorySegmentResult mmap = ipc_mmap(path, size);
  if (IpcMemorySegmentResult_is_error(mmap)) {
    error.sys_errno = mmap.error.body.sys_errno;
    return IpcInitBufferAttachResult_error_body(mmap.ipc_status,
                                                mmap.error.detail, error);
  }

  const IpcBufferAttachResult buffer_result = ipc_buffer_attach(mmap.result);
  if (IpcBufferAttachResult_is_error(buffer_result)) {
    return IpcInitBufferAttachResult_error_body(
        buffer_result.ipc_status, buffer_result.error.detail, error);
  }

  return IpcInitBufferAttachResult_ok(IPC_OK, buffer_result.result);
}

IpcInitChannelOpenResult
ipc_init_channel_create(const char *path, const size_t size,
                        const IpcChannelConfiguration config) {
  IpcInitChannelOpenError error = {.requested_size = size};

  const ValidationResult validation = _validate_size(
      size, ipc_channel_get_min_size(), ipc_channel_get_memory_overhead());

  if (validation.status != IPC_OK) {
    error.min_size = validation.min_size;
    error.overhead = validation.overhead;
    return IpcInitChannelOpenResult_error_body(validation.status,
                                               validation.error_msg, error);
  }

  const IpcMemorySegmentResult mmap = ipc_mmap(path, size);
  if (IpcMemorySegmentResult_is_error(mmap)) {
    error.sys_errno = mmap.error.body.sys_errno;
    return IpcInitChannelOpenResult_error_body(mmap.ipc_status,
                                               mmap.error.detail, error);
  }

  const IpcChannelOpenResult channel_open_result =
      ipc_channel_create(mmap.result, size, config);
  if (IpcChannelOpenResult_is_error(channel_open_result)) {
    error.sys_errno = channel_open_result.error.body.sys_errno;
    return IpcInitChannelOpenResult_error_body(channel_open_result.ipc_status,
                                               channel_open_result.error.detail,
                                               error);
  }

  return IpcInitChannelOpenResult_ok(IPC_OK, channel_open_result.result);
}

IpcInitChannelConnectResult
ipc_init_channel_connect(const char *path, const size_t size,
                         const IpcChannelConfiguration config) {
  IpcInitChannelConnectError error = {.requested_size = size};

  const ValidationResult validation = _validate_size(
      size, ipc_channel_get_min_size(), ipc_channel_get_memory_overhead());

  if (validation.status != IPC_OK) {
    error.min_size = validation.min_size;
    error.overhead = validation.overhead;
    return IpcInitChannelConnectResult_error_body(validation.status,
                                                  validation.error_msg, error);
  }

  const IpcMemorySegmentResult mmap = ipc_mmap(path, size);
  if (IpcMemorySegmentResult_is_error(mmap)) {
    error.sys_errno = mmap.error.body.sys_errno;
    return IpcInitChannelConnectResult_error_body(mmap.ipc_status,
                                                  mmap.error.detail, error);
  }

  const IpcChannelConnectResult channel_connect_result =
      ipc_channel_connect(mmap.result, config);

  if (IpcChannelConnectResult_is_error(channel_connect_result)) {
    return IpcInitChannelConnectResult_error_body(
        channel_connect_result.ipc_status, channel_connect_result.error.detail,
        error);
  }

  return IpcInitChannelConnectResult_ok(IPC_OK, channel_connect_result.result);
}

static inline uint64_t _find_next_power_of_2(uint64_t n) {
  if (n == 0) {
    return 1;
  }

  uint64_t pow = 1;
  while (pow < n) {
    pow <<= 1;
  }

  return pow;
}

static ValidationResult _validate_size(size_t requested_size, uint64_t min_size,
                                       uint64_t overhead) {
  ValidationResult result = {0};

  if (requested_size < min_size) {
    result.status = IPC_ERR_INVALID_ARGUMENT;
    result.error_msg = NOT_ENOUGH_SIZE_ERROR_MESSAGE;
    result.min_size = min_size;
    return result;
  }

  const uint64_t data_capacity = requested_size - overhead;
  if (!is_power_of_2(data_capacity)) {
    result.status = IPC_ERR_INVALID_ARGUMENT;
    result.error_msg = NOT_ALIGNED_ERROR_MESSAGE;
    result.overhead = overhead;
    return result;
  }

  result.status = IPC_OK;
  return result;
}
