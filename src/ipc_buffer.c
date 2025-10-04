#include "ipc_utils.h"
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_common.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IPC_DATA_ALIGN 8
#define MIN_BUFFER_SIZE                                                        \
  (sizeof(IpcBufferHeader) + 2) // 2 bytes min buffer size despite header

typedef uint8_t Flag;
static const Flag FLAG_NOT_READY = 0;
static const Flag FLAG_READY = 1;
static const Flag FLAG_WRAP_AROUND = 2;
static const Flag FLAG_LOCKED = 3;

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
  _Atomic Flag flag;
  uint64_t seq;
  uint64_t payload_size;
  uint64_t entry_size;
} EntryHeader;

static uint64_t _find_max_power_of_2(const uint64_t max);
static Flag _read_flag(const void *addr);
static void _set_flag(void *addr, const Flag flag);
static IpcStatus _read_entry_header(const struct IpcBuffer *buffer,
                                    const uint64_t head, EntryHeader **dest);

uint64_t ipc_buffer_align_size(size_t size) {
  return (size < 2 ? 2 : size) + sizeof(IpcBufferHeader);
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

  void *payload = NULL;
  IpcBufferReserveEntryResult reserve_result =
      ipc_buffer_reserve_entry(buffer, size, &payload);
  if (IpcBufferReserveEntryResult_is_error(reserve_result)) {
    /* заполняем детали ошибки */
    const uint64_t cap =
        atomic_load(&((struct IpcBuffer *)buffer)->header->data_size);
    const uint64_t head =
        atomic_load(&((struct IpcBuffer *)buffer)->header->head);
    const uint64_t tail =
        atomic_load(&((struct IpcBuffer *)buffer)->header->tail);
    const uint64_t used = tail - head;
    const uint64_t free_space = cap - used;

    error.buffer_size = cap;
    error.available_contiguous = free_space;
    if (IpcBufferReserveEntryResult_is_error_has_body(reserve_result.error)) {
      error.offset = reserve_result.error.body.offset;
    }

    return IpcBufferWriteResult_error_body(reserve_result.ipc_status,
                                           reserve_result.error.detail, error);
  }

  const uint64_t entry_offset = reserve_result.result;
  memcpy(payload, data, size);

  const IpcBufferCommitEntryResult commin_result =
      ipc_buffer_commit_entry(buffer, entry_offset);
  if (IpcBufferCommitEntryResult_is_error(commin_result)) {
    error.offset = entry_offset;
    return IpcBufferWriteResult_error_body(commin_result.ipc_status,
                                           "commit failed", error);
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

  for (;;) {
    head = atomic_load(&((struct IpcBuffer *)buffer)->header->head);

    EntryHeader *header = NULL;
    const IpcStatus status =
        _read_entry_header((struct IpcBuffer *)buffer, head, &header);
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
    memcpy(dest->payload, ((uint8_t *)header) + sizeof(EntryHeader),
           header->payload_size);
    if (atomic_compare_exchange_strong(
            &((struct IpcBuffer *)buffer)->header->head, &head,
            head + header->entry_size)) {
      dest->offset = head;
      dest->size = header->payload_size;
      return IpcBufferReadResult_ok(IPC_OK);
    }
  }
}

IpcBufferPeekResult ipc_buffer_peek(const IpcBuffer *buffer, IpcEntry *dest) {
  IpcBufferPeekError error = {.offset = 0};
  if (buffer == NULL) {
    return IpcBufferPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (dest == NULL) {
    return IpcBufferPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  const uint64_t head =
      atomic_load(&((const struct IpcBuffer *)buffer)->header->head);

  EntryHeader *header = NULL;
  const IpcStatus status =
      _read_entry_header((const struct IpcBuffer *)buffer, head, &header);
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

  uint64_t head;
  EntryHeader *header = NULL;
  Flag flag;

  for (;;) {
    head = atomic_load(&((struct IpcBuffer *)buffer)->header->head);
    if (head != offset) {
      error.offset = head;
      return IpcBufferSkipResult_error_body(
          IPC_ERR_OFFSET_MISMATCH,
          "Offset mismatch: expected different offset than current head",
          error);
    }

    const IpcStatus status =
        _read_entry_header((struct IpcBuffer *)buffer, head, &header);
    if (status == IPC_EMPTY) {
      return IpcBufferSkipResult_ok(IPC_EMPTY, head);
    }

    if (status == IPC_ERR_LOCKED) {
      error.offset = head;
      return IpcBufferSkipResult_error_body(IPC_ERR_LOCKED, "locked", error);
    }

    flag = _read_flag(&header->flag);
    if (atomic_compare_exchange_strong(&header->flag, &flag, FLAG_LOCKED)) {
      break;
    }
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

IpcBufferSkipForceResult ipc_buffer_skip_force(IpcBuffer *buffer) {
  IpcBufferSkipForceError error = {._unit = false};

  if (buffer == NULL) {
    return IpcBufferSkipForceResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  uint64_t head = atomic_load(&((struct IpcBuffer *)buffer)->header->head);
  EntryHeader *header = NULL;
  IpcStatus status =
      _read_entry_header((struct IpcBuffer *)buffer, head, &header);
  if (status == IPC_EMPTY) {
    return IpcBufferSkipForceResult_ok(IPC_EMPTY, head);
  }

  return atomic_compare_exchange_strong(
             &((struct IpcBuffer *)buffer)->header->head, &head,
             head + header->entry_size)
             ? IpcBufferSkipForceResult_ok(IPC_OK, head)
             : IpcBufferSkipForceResult_ok(IPC_ALREADY_SKIPPED, head);
}

IpcBufferReserveEntryResult
ipc_buffer_reserve_entry(IpcBuffer *buffer, const size_t size, void **dest) {
  IpcBufferReserveEntryError error = {.offset = 0};

  if (buffer == NULL) {
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  if (size == 0) {
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: allocation size is 0",
        error);
  }

  if (dest == NULL) {
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", error);
  }

  const uint64_t buf_size =
      atomic_load(&((struct IpcBuffer *)buffer)->header->data_size);
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buf_size) {
    error.buffer_size = buf_size;
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_ENTRY_TOO_LARGE, "invalid argument: entry size exceeds buffer",
        error);
  }

  uint64_t tail, rel_tail, space_to_wrap, total_required_entry_size;
  bool wrapped = false;

  do {
    tail = atomic_load(&((struct IpcBuffer *)buffer)->header->tail);
    rel_tail = RELATIVE(tail, buf_size);
    space_to_wrap = buf_size - rel_tail;

    wrapped = space_to_wrap < full_entry_size;
    total_required_entry_size = full_entry_size;
    if (wrapped) {
      total_required_entry_size =
          ALIGN_UP(space_to_wrap + sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
    }

    const uint64_t head =
        atomic_load(&((struct IpcBuffer *)buffer)->header->head);
    const uint64_t used = tail - head;
    const uint64_t free_space = buf_size - used;

    if (free_space < total_required_entry_size) {
      error.offset = tail;
      error.required_size = total_required_entry_size;
      error.free_space = free_space;
      return IpcBufferReserveEntryResult_error_body(
          IPC_ERR_NO_SPACE_CONTIGUOUS, "not enough contiguous space in buffer",
          error);
    }
  } while (!atomic_compare_exchange_strong(
      &((struct IpcBuffer *)buffer)->header->tail, &tail,
      tail + total_required_entry_size));

  uint64_t offset = rel_tail;
  if (wrapped) {
    _set_flag(((struct IpcBuffer *)buffer)->data + offset, FLAG_WRAP_AROUND);
    offset = 0;
  }

  EntryHeader *header =
      (EntryHeader *)(((struct IpcBuffer *)buffer)->data + offset);
  header->entry_size = total_required_entry_size;
  header->payload_size = size;
  _set_flag((uint8_t *)&header->flag, FLAG_NOT_READY);

  *dest = (void *)(((uint8_t *)header) + sizeof(EntryHeader));

  return IpcBufferReserveEntryResult_ok(IPC_OK, tail);
}

IpcBufferCommitEntryResult ipc_buffer_commit_entry(IpcBuffer *buffer,
                                                   const uint64_t offset) {
  IpcBufferCommitEntryError error = {.offset = offset};
  if (buffer == NULL) {
    return IpcBufferCommitEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", error);
  }

  EntryHeader *header = (EntryHeader *)(((struct IpcBuffer *)buffer)->data);
  const IpcStatus status =
      _read_entry_header((struct IpcBuffer *)buffer, offset, &header);

  // if not set NOT_READY flag yet, race condition guard
  if (status != IPC_ERR_NOT_READY && status != IPC_ERR_CORRUPTED) {
    error.offset = offset;

    if (status == IPC_ERR_LOCKED) {
      return IpcBufferCommitEntryResult_error_body(
          IPC_ERR_LOCKED, "locked: buffer is locked", error);
    }

    return IpcBufferCommitEntryResult_error_body(
        IPC_ERR_ILLEGAL_STATE,
        "illegal state: unexpected entry status, committed or flag incorrect",
        error);
  }

  header->seq = offset;
  _set_flag((uint8_t *)&header->flag, FLAG_READY);

  return IpcBufferCommitEntryResult_ok(IPC_OK);
}

static inline uint64_t _find_max_power_of_2(const uint64_t max) {
  uint64_t res = 1;
  while (res < max)
    res <<= 1;
  return res == max ? max : (res >> 1);
}

static inline Flag _read_flag(const void *addr) {
  const _Atomic Flag *atomic_flag = (_Atomic Flag *)addr;
  return atomic_load_explicit(atomic_flag, memory_order_acquire);
}

static inline void _set_flag(void *addr, const Flag flag) {
  _Atomic Flag *atomic_flag = (_Atomic Flag *)addr;
  atomic_store_explicit(atomic_flag, flag, memory_order_release);
}

static inline IpcStatus _read_entry_header(const struct IpcBuffer *buffer,
                                           const uint64_t head,
                                           EntryHeader **dest) {
  if (head == atomic_load(&buffer->header->tail)) {
    return IPC_EMPTY;
  }

  const uint64_t buf_size = atomic_load(&buffer->header->data_size);
  const uint64_t rel_head = RELATIVE(head, buf_size);

  Flag first_flag = _read_flag(buffer->data + rel_head);
  EntryHeader *header = (first_flag == FLAG_WRAP_AROUND)
                            ? (EntryHeader *)buffer->data
                            : (EntryHeader *)(buffer->data + rel_head);

  if (first_flag == FLAG_WRAP_AROUND) {
    first_flag = _read_flag(&header->flag);
  }

  // always set dest
  *dest = header;

  if (first_flag == FLAG_NOT_READY) {
    return IPC_ERR_NOT_READY;
  }

  if (first_flag == FLAG_LOCKED) {
    return IPC_ERR_LOCKED;
  }

  return (head != header->seq) ? IPC_ERR_CORRUPTED : IPC_OK;
}
