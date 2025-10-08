#include "ipc_utils.h"
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_common.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IPC_DATA_ALIGN 8
#define MIN_BUFFER_SIZE                                                        \
  (sizeof(IpcBufferHeader) +                                                   \
   IPC_DATA_ALIGN) // 8 bytes min buffer size despite header
#define UNLOCK(offset) (((offset) & (~(0x1))))
#define LOCK(offset) ((offset) | 1)

typedef struct IpcBufferHeader {
  _Atomic uint64_t head;
  _Atomic uint64_t tail;
  _Atomic uint64_t data_size;
} IpcBufferHeader;

struct IpcBuffer {
  IpcBufferHeader *header;
  uint8_t *data;
};

typedef struct EntryHeader {
  uint64_t seq;
  uint64_t payload_size;
  uint64_t entry_size;
} EntryHeader;

static uint64_t _find_max_power_of_2(const uint64_t max);
static uint64_t _read_head(const struct IpcBuffer *buffer);
static bool _is_aligned(const uint64_t offset);
static IpcStatus _read_entry_header(const struct IpcBuffer *buffer,
                                    const uint64_t head, EntryHeader **dest);

uint64_t ipc_buffer_align_size(size_t size) {
  return (size < IPC_DATA_ALIGN ? IPC_DATA_ALIGN : size) +
         sizeof(IpcBufferHeader);
}

IpcBufferCreateResult ipc_buffer_create(void *mem, const size_t size) {
  const IpcBufferCreateError error = {.requested_size = size,
                                      .min_size = MIN_BUFFER_SIZE};

  if (mem == NULL) {
    return IpcBufferCreateResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: mem is NULL", error);
  }

  if (size < MIN_BUFFER_SIZE) {
    return IpcBufferCreateResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer size too small",
        error);
  }

  struct IpcBuffer *buffer =
      (struct IpcBuffer *)malloc(sizeof(struct IpcBuffer));
  if (buffer == NULL) {
    return IpcBufferCreateResult_error_body(
        IPC_ERR_SYSTEM, "system error: buffer allocation failed", error);
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = ((uint8_t *)mem) + sizeof(IpcBufferHeader);

  const uint64_t cap = _find_max_power_of_2(size - sizeof(IpcBufferHeader));
  atomic_init(&buffer->header->data_size, cap);
  atomic_init(&buffer->header->head, 0);
  atomic_init(&buffer->header->tail, 0);

  return IpcBufferCreateResult_ok(IPC_OK, buffer);
}

IpcBufferAttachResult ipc_buffer_attach(void *mem) {
  const IpcBufferAttachError error = {.min_size = MIN_BUFFER_SIZE};
  if (mem == NULL) {
    return IpcBufferAttachResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: mem is NULL", error);
  }

  struct IpcBuffer *buffer =
      (struct IpcBuffer *)malloc(sizeof(struct IpcBuffer));

  if (buffer == NULL) {
    return IpcBufferAttachResult_error_body(
        IPC_ERR_SYSTEM, "system error: allocation failed", error);
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = ((uint8_t *)mem) + sizeof(IpcBufferHeader);

  return IpcBufferAttachResult_ok(IPC_OK, buffer);
}

IpcBufferWriteResult ipc_buffer_write(IpcBuffer *buffer, const void *data,
                                      const size_t size) {
  IpcBufferWriteError error = {.offset = 0,
                               .requested_size = size,
                               .available_contiguous = 0,
                               .buffer_size = 0};

  if (buffer == NULL) {
    return IpcBufferWriteResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (data == NULL) {
    return IpcBufferWriteResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: data is NULL", error);
  }
  if (size == 0) {
    return IpcBufferWriteResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: data size is 0", error);
  }

  const uint64_t buf_size =
      atomic_load(&((struct IpcBuffer *)buffer)->header->data_size);
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buf_size) {
    error.buffer_size = buf_size;
    return IpcBufferWriteResult_error_body(
        IPC_ERR_ENTRY_TOO_LARGE, "invalid argument: entry size exceeds buffer",
        error);
  }

  uint64_t tail, rel_tail, space_to_wrap;
  bool placeholder = false;
  do {
    tail = atomic_load(&((struct IpcBuffer *)buffer)->header->tail);
    if (tail == LOCK(tail)) {
      return IpcBufferWriteResult_error_body(IPC_ERR_LOCKED, "already reserved",
                                             error);
    }

    rel_tail = RELATIVE(tail, buf_size);

    space_to_wrap = buf_size - rel_tail;
    const uint64_t head = UNLOCK(_read_head(buffer));
    const uint64_t used = tail - head;
    const uint64_t free_space = buf_size - used;

    if (free_space < full_entry_size) {
      error.offset = tail;
      error.required_size = (size_t)(full_entry_size);
      error.free_space = (size_t)(free_space);
      return IpcBufferWriteResult_error_body(
          IPC_ERR_NO_SPACE_CONTIGUOUS, "not enough contiguous space in buffer",
          error);
    }

    // no space for current entry + header of next placeholder
    placeholder = space_to_wrap < full_entry_size + sizeof(EntryHeader);
  } while (!atomic_compare_exchange_strong(
      &((struct IpcBuffer *)buffer)->header->tail, &tail, LOCK(tail)));

  EntryHeader *header =
      (EntryHeader *)(((struct IpcBuffer *)buffer)->data + rel_tail);

  if (placeholder) {
    header->payload_size = 0;
    header->entry_size = space_to_wrap;
    header->seq = tail;
  } else {
    header->entry_size = full_entry_size;
    header->payload_size = size;
    header->seq = tail;
  }

  void *dest = (void *)(((uint8_t *)header) + sizeof(EntryHeader));
  memcpy(dest, data, size);

  uint64_t expected_offset = LOCK(tail);
  if (!atomic_compare_exchange_strong(
          &((struct IpcBuffer *)buffer)->header->tail, &expected_offset,
          tail + header->entry_size)) {
    error.offset = tail;
    return IpcBufferWriteResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected tail offset", error);
  }

  return IpcBufferWriteResult_ok(IPC_OK);
}

IpcBufferReadResult ipc_buffer_read(IpcBuffer *buffer, IpcEntry *dest) {
  IpcBufferReadError error = {.offset = 0};

  if (buffer == NULL) {
    return IpcBufferReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcBufferReadResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  const size_t dst_cap = dest->size;
  uint64_t head;
  EntryHeader *header = NULL;

  for (;;) {
    head = _read_head(buffer);
    const IpcStatus status =
        _read_entry_header((struct IpcBuffer *)buffer, head, &header);

    if (status == IPC_ERR_LOCKED) {
      error.offset = UNLOCK(head);
      return IpcBufferReadResult_error_body(status, "entry is locked", error);
    }

    if (status == IPC_PLACEHOLDER) {
      ipc_buffer_skip(buffer, head);
      continue;
    }

    if (status != IPC_OK) {
      if (status == IPC_EMPTY) {
        return IpcBufferReadResult_ok(IPC_EMPTY);
      }

      error.offset = head;
      return IpcBufferReadResult_error_body(status, "unreadable entry state",
                                            error);
    }

    if (dst_cap < header->payload_size) {
      error.offset = head;
      error.required_size = header->payload_size;
      return IpcBufferReadResult_error_body(
          IPC_ERR_TOO_SMALL, "destination buffer is too small", error);
    }

    // read/write race guard, read before move head
    if (atomic_compare_exchange_strong(
            &((struct IpcBuffer *)buffer)->header->head, &head, LOCK(head))) {
      break;
    }
  }

  uint64_t expected_current_head = LOCK(head);
  memcpy(dest->payload, ((uint8_t *)header) + sizeof(EntryHeader),
         header->payload_size);
  dest->offset = head;
  dest->size = header->payload_size;

  if (!atomic_compare_exchange_strong(
          &((struct IpcBuffer *)buffer)->header->head, &expected_current_head,
          head + header->entry_size)) {
    return IpcBufferReadResult_error_body(
        IPC_ERR_ILLEGAL_STATE, "illegal state: unexpected head offset", error);
  }

  return IpcBufferReadResult_ok(IPC_OK);
}

IpcBufferPeekResult ipc_buffer_peek(IpcBuffer *buffer, IpcEntry *dest) {
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
  EntryHeader *header = NULL;
  IpcStatus status;
  for (;;) {
    head = UNLOCK(_read_head(buffer));
    status = _read_entry_header((struct IpcBuffer *)buffer, head, &header);

    if (status != IPC_PLACEHOLDER) {
      break;
    }

    ipc_buffer_skip(buffer, head);
  }

  if (status != IPC_OK) {
    if (status == IPC_EMPTY) {
      return IpcBufferPeekResult_ok(IPC_EMPTY);
    }

    error.offset = head;
    return IpcBufferPeekResult_error_body(status, "unreadable buffer state",
                                          error);
  }

  dest->offset = head;
  dest->size = header->payload_size;
  dest->payload = (uint8_t *)header + sizeof(EntryHeader);

  return IpcBufferPeekResult_ok(IPC_OK);
}

IpcBufferSkipResult ipc_buffer_skip(IpcBuffer *buffer, const uint64_t offset) {
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
  EntryHeader *header = NULL;
  for (;;) {
    head = _read_head(buffer);
    if (head != offset) {
      error.offset = head;
      return IpcBufferSkipResult_error_body(
          IPC_ERR_OFFSET_MISMATCH,
          "Offset mismatch: expected different offset than current head",
          error);
    }

    const IpcStatus status =
        _read_entry_header((struct IpcBuffer *)buffer, head, &header);

    if (status == IPC_ERR_LOCKED) {
      error.offset = UNLOCK(head);
      return IpcBufferSkipResult_error_body(IPC_ERR_LOCKED, "entry is locked",
                                            error);
    }

    if (status == IPC_PLACEHOLDER) {
      atomic_compare_exchange_strong(
          &((struct IpcBuffer *)buffer)->header->head, &head,
          head + header->entry_size);
      continue;
    }

    if (status == IPC_EMPTY) {
      return IpcBufferSkipResult_ok(IPC_EMPTY, head);
    }

    if (!atomic_compare_exchange_strong(
            &((struct IpcBuffer *)buffer)->header->head, &head,
            head + header->entry_size)) {
      error.offset = head;
      return IpcBufferSkipResult_error_body(
          IPC_ERR_OFFSET_MISMATCH,
          "Offset mismatch: expected different offset than current head",
          error);
    }

    return IpcBufferSkipResult_ok(IPC_OK, offset);
  }
}

IpcBufferSkipForceResult ipc_buffer_skip_force(IpcBuffer *buffer) {
  IpcBufferSkipForceError error = {._unit = false};

  if (buffer == NULL) {
    return IpcBufferSkipForceResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  uint64_t head;
  EntryHeader *header = NULL;
  IpcStatus status;
  for (;;) {
    head = _read_head(buffer);
    status = _read_entry_header((struct IpcBuffer *)buffer, head, &header);

    if (status == IPC_ERR_LOCKED) {
      head = UNLOCK(head);
    }

    if (status != IPC_PLACEHOLDER) {
      break;
    }

    ipc_buffer_skip(buffer, head);
  }

  if (status == IPC_EMPTY) {
    return IpcBufferSkipForceResult_ok(IPC_EMPTY, head);
  }

  return atomic_compare_exchange_strong(
             &((struct IpcBuffer *)buffer)->header->head, &head,
             head + header->entry_size)
             ? IpcBufferSkipForceResult_ok(IPC_OK, head)
             : IpcBufferSkipForceResult_ok(IPC_ALREADY_SKIPPED, head);
}

static inline uint64_t _read_head(const struct IpcBuffer *buffer) {
  return atomic_load(&buffer->header->head);
}

static inline uint64_t _find_max_power_of_2(const uint64_t max) {
  uint64_t res = 1;
  while (res < max)
    res <<= 1;
  return res == max ? max : (res >> 1);
}

static inline bool _is_aligned(const uint64_t offset) {
  return ALIGN_UP(offset, IPC_DATA_ALIGN) == offset;
}

static inline IpcStatus _read_entry_header(const struct IpcBuffer *buffer,
                                           const uint64_t offset,
                                           EntryHeader **dest) {
  const uint64_t aligned_head = UNLOCK(offset);
  const uint64_t tail = atomic_load(&buffer->header->tail);
  if (aligned_head == tail) {
    return IPC_EMPTY;
  }

  if (LOCK(offset) == tail) {
    return IPC_ERR_NOT_READY;
  }

  const uint64_t buf_size = atomic_load(&buffer->header->data_size);
  const uint64_t rel_head = RELATIVE(aligned_head, buf_size);

  EntryHeader *header = (EntryHeader *)(buffer->data + rel_head);

  // always set dest
  *dest = header;

  const uint64_t seq = header->seq;
  if (seq == offset) {
    return header->payload_size == 0 ? IPC_PLACEHOLDER : IPC_OK;
  }

  if (offset == LOCK(seq)) {
    return IPC_ERR_LOCKED;
  }

  return IPC_ERR_CORRUPTED;
}
