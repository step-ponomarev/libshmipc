#include "ipc_utils.h"
#include <errno.h>
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_common.h>
#include "ipc_error_internal.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IPC_DATA_ALIGN 0x8

#define BUFFER_HEADER_SIZE_ALIGNED sizeof(ipc_buffer_header_t)
#define MIN_BUFFER_SIZE (BUFFER_HEADER_SIZE_ALIGNED + IPC_DATA_ALIGN)
#define UNLOCK(offset) (((offset) & (~(0x1))))
#define LOCK(offset) ((offset) | 0x1)

typedef struct ipc_buffer_header_t {
  _Atomic uint64_t head;
  _Atomic uint64_t data_size;
  uint8_t _r_padding[64 - 2 * sizeof(uint64_t)];

  _Atomic uint64_t tail;
  uint8_t _w_padding[64 - sizeof(uint64_t)];
} ipc_buffer_header_t;

struct ipc_buffer_t {
  ipc_buffer_header_t *header;
  uint8_t *data;
};

typedef struct EntryHeader {
  uint64_t seq;
  uint64_t payload_size;
  uint64_t entry_size;
} EntryHeader;

static uint64_t _read_head(const ipc_buffer_t *buffer);
static bool _is_aligned(uint64_t offset);
static bool _lock(_Atomic uint64_t *ref, uint64_t offset);
static bool _unlock(_Atomic uint64_t *ref, uint64_t offset);
static bool _is_locked(const uint64_t offset);
static IpcStatus _read_entry_header_unsafe(const ipc_buffer_t *buffer,
                                           uint64_t offset,
                                           EntryHeader **dest);
static IpcStatus _read_entry_header(const ipc_buffer_t *buffer,
                                    uint64_t offset, EntryHeader *dest);

inline uint64_t ipc_buffer_get_memory_overhead(void) {
  return BUFFER_HEADER_SIZE_ALIGNED; // TODO: rename to min size
}

inline uint64_t ipc_buffer_get_min_size(void) {
  return MIN_BUFFER_SIZE;
}

uint64_t ipc_buffer_suggest_size(size_t desired_capacity) {
  const uint64_t overhead = ipc_buffer_get_memory_overhead();

  if (desired_capacity + overhead < MIN_BUFFER_SIZE) {
    return MIN_BUFFER_SIZE;
  }

  const uint64_t aligned_capacity = find_next_power_of_2(desired_capacity);
  return aligned_capacity + overhead;
}

// TODO: sycnhronize create/attach logic: race condition
//TODO: check size mem is aligned
ipc_status_t ipc_buffer_create(void *mem, size_t size, ipc_buffer_t **out, ipc_error_t* err) {
  ipc_error_init(err);
  ipc_buffer_t *res = NULL;

  if (out == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
  }

  *out = NULL;

  if (mem == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
  }

  if (size < MIN_BUFFER_SIZE) {
    return ipc_error_arg_size(
      err,
      IPC_ERR_CODE_TOO_SMALL_SIZE,
      (ipc_error_size_t){.provided_size = size, .min_size = MIN_BUFFER_SIZE, .suggested_size = MIN_BUFFER_SIZE},
      "buffer size too small, use ipc_buffer_suggest_size"
    );
  }

  const uint64_t data_capacity = size - BUFFER_HEADER_SIZE_ALIGNED;
  if (!is_power_of_2(data_capacity)) {
    return ipc_error_arg_size(
      err,
      IPC_ERR_CODE_INVALID_CAPACITY,
      (ipc_error_size_t){
        .provided_size = size, .min_size = MIN_BUFFER_SIZE, .suggested_size = ipc_buffer_suggest_size(data_capacity)
      },
      "size must be power of 2, use ipc_buffer_suggest_size"
    );
  }

  res = (ipc_buffer_t *)malloc(sizeof(ipc_buffer_t));
  if (res == NULL) {
    return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "buffer allocation failed", errno);
  }

  res->header = (ipc_buffer_header_t *)mem;
  res->data = (uint8_t *)mem + BUFFER_HEADER_SIZE_ALIGNED;

  atomic_init(&res->header->data_size, data_capacity);
  atomic_init(&res->header->head, 0);
  atomic_init(&res->header->tail, 0);

  *out = res;

  return IPC_STATUS_OK;
}

SHMIPC_API ipc_status_t ipc_buffer_attach(void *mem, ipc_buffer_t **out, ipc_error_t* err) {
  ipc_error_init(err);
  ipc_buffer_t *res = NULL;

  if (out == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "out is null");
  }

  *out = NULL;

  if (mem == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "mem is null");
  }

  res = (ipc_buffer_t *)malloc(sizeof(ipc_buffer_t));
  if (res == NULL) {
    return ipc_error_sys(err, IPC_ERR_CODE_ALLOCATION, "buffer allocation failed", errno);
  }

  res->header = (ipc_buffer_header_t *)mem;
  res->data = (uint8_t *)mem + BUFFER_HEADER_SIZE_ALIGNED;

  *out = res;

  return IPC_STATUS_OK;
}

ipc_status_t ipc_buffer_write(ipc_buffer_t *buffer, const void *data, size_t size, ipc_error_t* err) {
  ipc_error_init(err);

  if (buffer == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "buffer is null");
  }

  if (data == NULL) {
    return ipc_error_arg(err, IPC_ERR_CODE_NULL_ARG, "data is null");
  }

  if (size == 0) {
    return ipc_error_arg(err, IPC_ERR_CODE_ZERO_SIZE, "data size is 0");
  }

  retry:;
  const uint64_t buf_size = atomic_load(&buffer->header->data_size);
  const uint64_t full_entry_size = ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buf_size) {
    return ipc_error_arg(err, IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER, "entry size exceeds buffer");
  }

  uint64_t tail, rel_tail, space_to_wrap;
  bool placeholder = false;
  do {
    tail = atomic_load(&buffer->header->tail);
    if (_is_locked(tail)) {
      return IPC_STATUS_BUSY;
    }

    rel_tail = RELATIVE(tail, buf_size);

    space_to_wrap = buf_size - rel_tail;
    const uint64_t head = UNLOCK(_read_head(buffer));
    const uint64_t used = tail - head;
    const uint64_t free_space = buf_size - used;

    if (free_space < full_entry_size) {
      return IPC_STATUS_NO_SPACE;
    }

    // no space for current entry + header of next placeholder
    placeholder = space_to_wrap < full_entry_size + sizeof(EntryHeader);
  } while (!_lock(&buffer->header->tail, tail));

  EntryHeader *header = (EntryHeader *)(buffer->data + rel_tail);
  if (placeholder) {
    header->payload_size = 0;
    header->entry_size = space_to_wrap;
  } else {
    void *dest = (uint8_t *)header + sizeof(EntryHeader);
    memcpy(dest, data, size);
    header->entry_size = full_entry_size;
    header->payload_size = size;
  }
  header->seq = tail;

  uint64_t expected_offset = LOCK(tail);
  if (!atomic_compare_exchange_strong(&buffer->header->tail, &expected_offset, tail + header->entry_size)) {
    return ipc_error_internal_cas(
      err,
      IPC_ERR_CODE_OFFSET_CAS_FAILED,
      "unexpected tail value during commit",
      (ipc_error_cas_t){
        .target = IPC_CAS_TARGET_TAIL, .expected_offset = LOCK(tail), .actual_offset = expected_offset,
        .desired_offset = tail + header->entry_size
      }
    );
  }

  if (placeholder) {
    goto retry;
  }

  return IPC_STATUS_OK;
}

IpcBufferReadResult ipc_buffer_read(ipc_buffer_t *buffer, IpcEntry *dest) {
  IpcBufferReadError error = {.offset = 0};

  if (buffer == NULL) {
    return IpcBufferReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcBufferReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  uint64_t head;
  do {
    head = _read_head(buffer);
    if (_is_locked(head)) {
      error.offset = UNLOCK(head);
      return IpcBufferReadResult_error_body(IPC_ERR_LOCKED, "entry is locked",
                                            error);
    }

  } while (!_lock(&((struct ipc_buffer_t *)buffer)->header->head, head));

  const size_t dst_cap = dest->size;
  EntryHeader header;

  const IpcStatus status =
      _read_entry_header((struct ipc_buffer_t *)buffer, head, &header);
  const bool placeholder = status == IPC_PLACEHOLDER;
  if (!placeholder && status != IPC_OK) {
    if (!_unlock(&((struct ipc_buffer_t *)buffer)->header->head, head)) {
      return IpcBufferReadResult_error_body(
          IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset",
          error);
    }

    if (status == IPC_EMPTY) {
      return IpcBufferReadResult_ok(IPC_EMPTY);
    }

    error.offset = head;
    return IpcBufferReadResult_error_body(status, "unreadable entry state",
                                          error);
  }

  const uint64_t rel_offset = RELATIVE(
      head, atomic_load(&((struct ipc_buffer_t *)buffer)->header->data_size));

  if (!placeholder) {
    if (dst_cap < header.payload_size) {
      error.offset = head;
      error.required_size = header.payload_size;
      if (!_unlock(&((struct ipc_buffer_t *)buffer)->header->head, head)) {
        return IpcBufferReadResult_error_body(
            IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset",
            error);
      }

      return IpcBufferReadResult_error_body(
          IPC_ERR_TOO_SMALL, "destination buffer is too small", error);
    }

    memcpy(dest->payload,
           ((((struct ipc_buffer_t *)buffer)->data + rel_offset) +
            sizeof(EntryHeader)),
           header.payload_size);
    dest->offset = head;
    dest->size = header.payload_size;
  }

  uint64_t expected_current_head = LOCK(head);
  if (!atomic_compare_exchange_strong(
          &((struct ipc_buffer_t *)buffer)->header->head, &expected_current_head,
          head + header.entry_size)) {
    return IpcBufferReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset", error);
  }

  return placeholder ? ipc_buffer_read(buffer, dest)
                     : IpcBufferReadResult_ok(IPC_OK);
}

IpcBufferPeekResult ipc_buffer_peek(ipc_buffer_t *buffer, IpcEntry *dest) {
  IpcBufferPeekError error = {.offset = 0};
  if (buffer == NULL) {
    return IpcBufferPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcBufferPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  uint64_t head;
  do {
    head = _read_head(buffer);
    if (_is_locked(head)) {
      error.offset = UNLOCK(head);
      return IpcBufferPeekResult_error_body(IPC_ERR_LOCKED, "entry is locked",
                                            error);
    }

  } while (!_lock(&((struct ipc_buffer_t *)buffer)->header->head, head));

  EntryHeader header;
  const IpcStatus status =
      _read_entry_header((struct ipc_buffer_t *)buffer, head, &header);
  const bool placeholder = status == IPC_PLACEHOLDER;

  if (!placeholder && status != IPC_OK) {
    if (!_unlock(&((struct ipc_buffer_t *)buffer)->header->head, head)) {
      return IpcBufferPeekResult_error_body(
          IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset",
          error);
    }

    if (status == IPC_EMPTY) {
      return IpcBufferPeekResult_ok(IPC_EMPTY);
    }

    error.offset = head;
    return IpcBufferPeekResult_error_body(status, "unreadable entry state",
                                          error);
  }

  if (placeholder) {
    uint64_t expected_current_head = LOCK(head);
    if (!atomic_compare_exchange_strong(
            &((struct ipc_buffer_t *)buffer)->header->head, &expected_current_head,
            head + header.entry_size)) {
      return IpcBufferPeekResult_error_body(
          IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset",
          error);
    }

    return ipc_buffer_peek(buffer, dest);
  }

  dest->offset = head;
  dest->size = header.payload_size;

  const uint64_t rel_offset = RELATIVE(
      head, atomic_load(&((struct ipc_buffer_t *)buffer)->header->data_size));
  dest->payload =
      ((((struct ipc_buffer_t *)buffer)->data + rel_offset) + sizeof(EntryHeader));

  if (!_unlock(&((struct ipc_buffer_t *)buffer)->header->head, head)) {
    return IpcBufferPeekResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset", error);
  }

  return IpcBufferPeekResult_ok(IPC_OK);
}

IpcBufferSkipResult ipc_buffer_skip(ipc_buffer_t *buffer, const uint64_t offset) {
  IpcBufferSkipError error = {.offset = offset};

  if (buffer == NULL) {
    return IpcBufferSkipResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (!_is_aligned(offset)) {
    return IpcBufferSkipResult_error_body(
        IPC_ERR_INVALID_ARGUMENT,
        "invalid argument: offset must be multiple of 8", error);
  }

  uint64_t head;
  do {
    head = _read_head(buffer);
    if (_is_locked(head)) {
      error.offset = UNLOCK(head);
      return IpcBufferSkipResult_error_body(IPC_ERR_LOCKED, "entry is locked",
                                            error);
    }

    if (UNLOCK(head) != offset) {
      error.offset = UNLOCK(head);
      return IpcBufferSkipResult_error_body(
          IPC_ERR_OFFSET_MISMATCH,
          "Offset mismatch: expected different offset than current head",
          error);
    }
  } while (!_lock(&((struct ipc_buffer_t *)buffer)->header->head, head));

  EntryHeader header;
  const IpcStatus status =
      _read_entry_header((struct ipc_buffer_t *)buffer, head, &header);
  const bool placeholder = status == IPC_PLACEHOLDER;
  if (!placeholder && status != IPC_OK) {
    if (!_unlock(&((struct ipc_buffer_t *)buffer)->header->head, head)) {
      return IpcBufferSkipResult_error_body(
          IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset",
          error);
    }

    if (status == IPC_EMPTY) {
      return IpcBufferSkipResult_ok(IPC_EMPTY, head);
    }

    error.offset = head;
    return IpcBufferSkipResult_error_body(status, "unreadable entry state",
                                          error);
  }

  uint64_t expected_current_head = LOCK(head);
  if (!atomic_compare_exchange_strong(
          &((struct ipc_buffer_t *)buffer)->header->head, &expected_current_head,
          head + header.entry_size)) {
    return IpcBufferSkipResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset", error);
  }

  return placeholder ? ipc_buffer_skip(buffer, offset)
                     : IpcBufferSkipResult_ok(IPC_OK, offset);
}

IpcBufferSkipForceResult ipc_buffer_skip_force(ipc_buffer_t *buffer) {
  IpcBufferSkipForceError error = {._unit = false};

  if (buffer == NULL) {
    return IpcBufferSkipForceResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  uint64_t head = UNLOCK(_read_head(buffer));
  EntryHeader *header = NULL;
  uint64_t entry_size;
  IpcStatus status = _read_entry_header_unsafe(buffer, head, &header);

  if (status == IPC_EMPTY) {
    return IpcBufferSkipForceResult_ok(IPC_EMPTY, head);
  }

  entry_size = header->entry_size;
  return atomic_compare_exchange_strong(
             &((struct ipc_buffer_t *)buffer)->header->head, &head,
             head + entry_size)
             ? IpcBufferSkipForceResult_ok(IPC_OK, head)
             : IpcBufferSkipForceResult_ok(IPC_ALREADY_SKIPPED, head);
}

static inline uint64_t _read_head(const struct ipc_buffer_t *buffer) {
  return atomic_load(&buffer->header->head);
}

static inline bool _is_aligned(const uint64_t offset) {
  return ALIGN_UP(offset, IPC_DATA_ALIGN) == offset;
}

static bool _is_locked(const uint64_t offset) { return LOCK(offset) == offset; }

static inline bool _lock(_Atomic uint64_t *ref, const uint64_t offset) {
  uint64_t expected = UNLOCK(offset);
  return atomic_compare_exchange_strong(ref, &expected, LOCK(offset));
}

static inline bool _unlock(_Atomic uint64_t *ref, const uint64_t offset) {
  uint64_t expected = LOCK(offset);
  return atomic_compare_exchange_strong(ref, &expected, UNLOCK(offset));
}

static IpcStatus _read_entry_header(const struct ipc_buffer_t *buffer,
                                    const uint64_t offset, EntryHeader *dest) {
  EntryHeader *header;
  IpcStatus status = _read_entry_header_unsafe(buffer, offset, &header);
  if (status != IPC_OK) {
    return status;
  }

  dest->seq = header->seq;
  if (dest->seq == offset) {
    dest->payload_size = header->payload_size;
    dest->entry_size = header->entry_size;

    if (dest->payload_size == 0) {
      return IPC_PLACEHOLDER;
    }

    return IPC_OK;
  }

  if (_is_locked(offset)) {
    return IPC_ERR_LOCKED;
  }

  return IPC_ERR_NOT_READY;
}

static IpcStatus _read_entry_header_unsafe(const struct ipc_buffer_t *buffer,
                                           const uint64_t offset,
                                           EntryHeader **dest) {
  const uint64_t aligned_head = UNLOCK(offset);
  const uint64_t tail = atomic_load(&buffer->header->tail);
  if (aligned_head == UNLOCK(tail)) {
    return IPC_EMPTY;
  }

  const uint64_t buf_size = atomic_load(&buffer->header->data_size);
  const uint64_t rel_head = RELATIVE(aligned_head, buf_size);

  *dest = (EntryHeader *)(buffer->data + rel_head);

  if (_is_locked(tail)) {
    return IPC_ERR_NOT_READY;
  }

  return IPC_OK;
}
