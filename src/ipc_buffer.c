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

  const Flag first_flag = _read_flag(buffer->data + rel_head);
  EntryHeader *header = (first_flag == FLAG_WRAP_AROUND)
                            ? (EntryHeader *)buffer->data
                            : (EntryHeader *)(buffer->data + rel_head);

  // always set dest
  *dest = header;

  if (first_flag == FLAG_NOT_READY)
    return IPC_ERR_NOT_READY;
  if (first_flag == FLAG_LOCKED)
    return IPC_ERR_LOCKED;

  return (head != header->seq) ? IPC_ERR_CORRUPTED : IPC_OK;
}

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
  IpcBufferWriteError error = {.entry_id = 0,
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
      error.entry_id = reserve_result.error.body.entry_id;
    }

    return IpcBufferWriteResult_error_body(reserve_result.ipc_status,
                                           reserve_result.error.detail, error);
  }

  const IpcEntryId entry_id = reserve_result.result;
  memcpy(payload, data, size);

  const IpcBufferCommitEntryResult commin_result =
      ipc_buffer_commit_entry(buffer, entry_id);
  if (IpcBufferCommitEntryResult_is_error(commin_result)) {
    error.entry_id = entry_id;
    return IpcBufferWriteResult_error_body(commin_result.ipc_status,
                                           "commit failed", error);
  }

  return IpcBufferWriteResult_ok(IPC_OK);
}

IpcBufferReadResult ipc_buffer_read(IpcBuffer *buffer, IpcEntry *dest) {
  IpcBufferReadError error = {.entry_id = 0};

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
  uint64_t full_entry_size;

  do {
    head = atomic_load(&((struct IpcBuffer *)buffer)->header->head);

    EntryHeader *hdr = NULL;
    const IpcStatus st =
        _read_entry_header((struct IpcBuffer *)buffer, head, &hdr);
    if (st != IPC_OK) {
      if (st == IPC_EMPTY) {
        /* non-error: ok c флагом EMPTY + выставим id и обнулим поля */
        dest->id = head;
        dest->size = 0;
        return IpcBufferReadResult_ok(IPC_EMPTY);
      }
      /* только ERR-коды — в error */
      IpcBufferReadError body = {.entry_id = head};
      return IpcBufferReadResult_error_body(st, "unreadable entry state", body);
    }

    full_entry_size = hdr->entry_size;
    dest->id = head;
    dest->size = hdr->payload_size;

    if (dst_cap < dest->size) {
      IpcBufferReadError body = {.entry_id = head};
      return IpcBufferReadResult_error_body(
          IPC_ERR_TOO_SMALL, "destination buffer is too small", body);
    }

    uint8_t *payload = ((uint8_t *)hdr) + sizeof(EntryHeader);
    memcpy(dest->payload, payload, hdr->payload_size);
  } while (!atomic_compare_exchange_strong(
      &((struct IpcBuffer *)buffer)->header->head, &head,
      head + full_entry_size));

  return IpcBufferReadResult_ok(IPC_OK);
}

/* === peek: 1-в-1 как раньше; EMPTY -> ok(EMPTY), ERR -> error === */
IpcBufferPeekResult ipc_buffer_peek(const IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL) {
    IpcBufferPeekError body = {.entry_id = 0};
    return IpcBufferPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", body);
  }
  if (dest == NULL) {
    IpcBufferPeekError body = {.entry_id = 0};
    return IpcBufferPeekResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is NULL", body);
  }

  const uint64_t head =
      atomic_load(&((const struct IpcBuffer *)buffer)->header->head);

  EntryHeader *hdr = NULL;
  const IpcStatus st =
      _read_entry_header((const struct IpcBuffer *)buffer, head, &hdr);
  if (st != IPC_OK) {
    if (st == IPC_EMPTY) {
      dest->id = head;
      dest->size = 0;
      return IpcBufferPeekResult_ok(IPC_EMPTY);
    }
    IpcBufferPeekError body = {.entry_id = head};
    return IpcBufferPeekResult_error_body(st, "unreadable buffer state", body);
  }

  dest->id = head;
  dest->size = hdr->payload_size;
  dest->payload = (uint8_t *)hdr + sizeof(EntryHeader);
  return IpcBufferPeekResult_ok(IPC_OK);
}

/* === skip: head!=id -> ok(ALREADY_SKIPPED), LOCKED -> error, EMPTY ->
 * ok(EMPTY) === */
IpcBufferSkipResult ipc_buffer_skip(IpcBuffer *buffer, const IpcEntryId id) {
  if (buffer == NULL) {
    IpcBufferSkipError body = {.entry_id = id};
    return IpcBufferSkipResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", body);
  }

  uint64_t head;
  EntryHeader *hdr = NULL;
  Flag flag;

  do {
    head = atomic_load(&((struct IpcBuffer *)buffer)->header->head);
    if (head != id) {
      return IpcBufferSkipResult_error_body(
          IPC_ERR_TRANSACTION_MISMATCH,
          "Transaction ID mismatch: expected different ID than current head",
          (IpcBufferSkipError){.entry_id = head});
    }

    const IpcStatus st =
        _read_entry_header((struct IpcBuffer *)buffer, head, &hdr);
    if (st == IPC_EMPTY) {
      return IpcBufferSkipResult_ok(IPC_EMPTY, head);
    }
    if (st == IPC_ERR_LOCKED) {
      IpcBufferSkipError body = {.entry_id = head};
      return IpcBufferSkipResult_error_body(IPC_ERR_LOCKED, "locked", body);
    }

    flag = _read_flag((uint8_t *)hdr);
  } while (!atomic_compare_exchange_strong(&hdr->flag, &flag, FLAG_LOCKED));
  /* залочили — двигаем head как раньше */

  const bool moved = atomic_compare_exchange_strong(
      &((struct IpcBuffer *)buffer)->header->head, &head,
      head + hdr->entry_size);
  const IpcStatus st = moved ? IPC_OK : IPC_ALREADY_SKIPPED;
  return IpcBufferSkipResult_ok(st, id);
}

IpcBufferSkipForceResult ipc_buffer_skip_force(IpcBuffer *buffer) {
  if (buffer == NULL) {
    IpcBufferSkipForceError body = {.entry_id = 0};
    return IpcBufferSkipForceResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", body);
  }

  uint64_t head = atomic_load(&((struct IpcBuffer *)buffer)->header->head);
  EntryHeader *hdr = NULL;
  IpcStatus st = _read_entry_header((struct IpcBuffer *)buffer, head, &hdr);
  if (st == IPC_EMPTY) {
    return IpcBufferSkipForceResult_ok(IPC_EMPTY, head);
  }

  const bool moved = atomic_compare_exchange_strong(
      &((struct IpcBuffer *)buffer)->header->head, &head,
      head + hdr->entry_size);
  st = moved ? IPC_OK : IPC_ALREADY_SKIPPED;
  return IpcBufferSkipForceResult_ok(st, head);
}

IpcBufferReserveEntryResult
ipc_buffer_reserve_entry(IpcBuffer *buffer, const size_t size, void **dest) {
  if (buffer == NULL) {
    IpcBufferReserveEntryError body = {.entry_id = 0};
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is NULL", body);
  }
  if (size == 0) {
    IpcBufferReserveEntryError body = {.entry_id = 0};
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: allocation size is 0",
        body);
  }
  if (dest == NULL) {
    IpcBufferReserveEntryError body = {.entry_id = 0};
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: dest is null", body);
  }

  const uint64_t buf_size =
      atomic_load(&((struct IpcBuffer *)buffer)->header->data_size);
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buf_size) {
    IpcBufferReserveEntryError body = {.entry_id = 0};
    return IpcBufferReserveEntryResult_error_body(
        IPC_ERR_ENTRY_TOO_LARGE, "invalid argument: entry size exceeds buffer",
        body);
  }

  uint64_t tail, rel_tail, space_to_wrap;
  bool wrapped = false;

  do {
    tail = atomic_load(&((struct IpcBuffer *)buffer)->header->tail);
    rel_tail = RELATIVE(tail, buf_size);
    space_to_wrap = buf_size - rel_tail;

    wrapped = space_to_wrap < full_entry_size;
    uint64_t effective_entry = full_entry_size;
    if (wrapped) {
      /* как раньше — увеличиваем, чтобы поместить маркер + запись с начала */
      effective_entry =
          ALIGN_UP(space_to_wrap + sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
    }

    const uint64_t head =
        atomic_load(&((struct IpcBuffer *)buffer)->header->head);
    const uint64_t used = tail - head;
    const uint64_t free_space = buf_size - used;

    if (free_space < effective_entry) {
      IpcBufferReserveEntryError body = {.entry_id = tail};
      return IpcBufferReserveEntryResult_error_body(
          IPC_ERR_NO_SPACE_CONTIGUOUS, "not enough contiguous space in buffer",
          body);
    }

    const uint64_t new_tail = tail + effective_entry;
    if (!atomic_compare_exchange_strong(
            &((struct IpcBuffer *)buffer)->header->tail, &tail, new_tail)) {
      continue; // retry
    }

    uint64_t offset = rel_tail;
    if (wrapped) {
      _set_flag(((struct IpcBuffer *)buffer)->data + offset, FLAG_WRAP_AROUND);
      offset = 0;
    }

    EntryHeader *hdr =
        (EntryHeader *)(((struct IpcBuffer *)buffer)->data + offset);
    hdr->entry_size = effective_entry;
    hdr->payload_size = size;
    _set_flag((uint8_t *)&hdr->flag, FLAG_NOT_READY);

    *dest = (void *)(((uint8_t *)hdr) + sizeof(EntryHeader));
    return IpcBufferReserveEntryResult_ok(IPC_OK, tail);

  } while (true);
}

IpcBufferCommitEntryResult ipc_buffer_commit_entry(IpcBuffer *buffer,
                                                   const IpcEntryId id) {
  if (buffer == NULL) {
    IpcBufferCommitEntryError body = {.entry_id = id};
    return IpcBufferCommitEntryResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: buffer is null", body);
  }

  EntryHeader *hdr = (EntryHeader *)(((struct IpcBuffer *)buffer)->data);
  const IpcStatus st = _read_entry_header((struct IpcBuffer *)buffer, id, &hdr);

  /* строго как раньше: коммит допустим только из NOT_READY/ CORRUPTED */
  if (st != IPC_ERR_NOT_READY && st != IPC_ERR_CORRUPTED) {
    if (st == IPC_ERR_LOCKED) {
      IpcBufferCommitEntryError body = {.entry_id = id};
      return IpcBufferCommitEntryResult_error_body(
          IPC_ERR_LOCKED, "locked: buffer is locked", body);
    }
    IpcBufferCommitEntryError body = {.entry_id = id};
    return IpcBufferCommitEntryResult_error_body(
        IPC_ERR_ILLEGAL_STATE,
        "illegal state: unexpected entry status, committed or flag incorrect",
        body);
  }

  hdr->seq = id;
  _set_flag((uint8_t *)&hdr->flag, FLAG_READY);
  return IpcBufferCommitEntryResult_ok(IPC_OK);
}
