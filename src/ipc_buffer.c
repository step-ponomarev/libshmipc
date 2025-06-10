#include "ipc_utils.h"
#include <shmipc/ipc_buffer.h>
#include <shmipc/ipc_common.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO: Optimize memory barier using

#define IPC_DATA_ALIGN 8

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

typedef struct IpcBuffer {
  IpcBufferHeader *header;
  uint8_t *data;
} IpcBuffer;

typedef struct EntryHeader {
  _Atomic Flag flag;
  uint64_t seq;
  uint64_t payload_size;
  uint64_t entry_size;
} EntryHeader;

uint64_t _find_max_power_of_2(const uint64_t);
IpcStatus _read_entry_header(const IpcBuffer *, const uint64_t, EntryHeader **);
Flag _read_flag(const void *);
void _set_flag(void *, const Flag);
IpcStatus _ipc_buffer_skip(IpcBuffer *, const IpcEntryId, const bool);

inline uint64_t ipc_buffer_allign_size(uint64_t size) {
  return size + sizeof(IpcBufferHeader);
}

IpcBuffer *ipc_buffer_create(void *mem, const uint64_t size) {
  if (mem == NULL || size <= sizeof(IpcBufferHeader) ||
      size - sizeof(IpcBufferHeader) < 2) {
    return NULL;
  }

  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    return NULL;
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = ((uint8_t *)mem) + sizeof(IpcBufferHeader);

  atomic_init(&buffer->header->data_size,
              _find_max_power_of_2(size - sizeof(IpcBufferHeader)));
  atomic_init(&buffer->header->head, 0);
  atomic_init(&buffer->header->tail, 0);

  return buffer;
}

IpcBuffer *ipc_buffer_attach(void *mem) {
  if (mem == NULL) {
    return NULL;
  }

  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    return NULL;
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = ((uint8_t *)mem) + sizeof(IpcBufferHeader);

  return buffer;
}

IpcStatus ipc_buffer_write(IpcBuffer *buffer, const void *data,
                           const uint64_t size) {
  if (buffer == NULL || data == NULL || size == 0) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  void *payload;
  IpcTransaction tx = ipc_buffer_reserve_entry(buffer, size, &payload);
  if (tx.status != IPC_OK) {
    return tx.status;
  }

  memcpy(payload, data, size);
  return ipc_buffer_commit_entry(buffer, tx.entry_id);
}

IpcTransaction ipc_buffer_read(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  const uint64_t buffer_size = buffer->header->data_size;
  const uint64_t dest_size = dest->size;
  uint64_t head;
  uint64_t full_entry_size;

  do {
    head = atomic_load(&buffer->header->head);

    EntryHeader *header;
    const IpcStatus status = _read_entry_header(buffer, head, &header);
    if (status != IPC_OK) {
      return ipc_create_transaction(head, status);
    }

    full_entry_size = header->entry_size;
    dest->size = header->payload_size;
    if (dest_size < dest->size) {
      return ipc_create_transaction(head, IPC_ERR_TOO_SMALL);
    }

    uint8_t *payload = ((uint8_t *)header) + sizeof(EntryHeader);
    memcpy(dest->payload, payload, header->payload_size);
  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + full_entry_size));

  return ipc_create_transaction(head, IPC_OK);
}

IpcTransaction ipc_buffer_peek(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  const uint64_t head = atomic_load(&buffer->header->head);

  EntryHeader *header;
  const IpcStatus status = _read_entry_header(buffer, head, &header);
  if (status != IPC_OK) {
    return ipc_create_transaction(head, status);
  }

  dest->size = header->payload_size;
  dest->payload = (uint8_t *)header + sizeof(EntryHeader);

  return ipc_create_transaction(head, IPC_OK);
}

IpcStatus ipc_buffer_skip(IpcBuffer *buffer, const IpcEntryId id) {
  return _ipc_buffer_skip(buffer, id, false);
}

IpcStatus ipc_buffer_skip_force(IpcBuffer *buffer) {
  return _ipc_buffer_skip(buffer, 0, true);
}

IpcTransaction ipc_buffer_reserve_entry(IpcBuffer *buffer, const uint64_t size,
                                        void **dest) {
  if (buffer == NULL || size == 0) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_ARGUMENT);
  }

  const uint64_t buffer_size = buffer->header->data_size;
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buffer_size) {
    return ipc_create_transaction(0, IPC_ERR_INVALID_SIZE);
  }

  uint64_t tail;
  uint64_t relative_tail;
  uint64_t space_to_wrap;
  do {
    tail = atomic_load(&buffer->header->tail);
    relative_tail = RELATIVE(tail, buffer_size);
    // to move header on start
    space_to_wrap = buffer_size - relative_tail;
    if (space_to_wrap < full_entry_size) {
      full_entry_size =
          ALIGN_UP(space_to_wrap + sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
    }

    uint64_t free_space = buffer_size - (tail - buffer->header->head);
    if (free_space < full_entry_size) {
      free_space = buffer_size - (tail - atomic_load(&buffer->header->head));
      if (free_space < full_entry_size) {
        return ipc_create_transaction(tail, IPC_NO_SPACE_CONTIGUOUS);
      }
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->tail, &tail,
                                           tail + full_entry_size));

  bool wrapped = false;
  uint64_t offset = relative_tail;
  if (space_to_wrap < full_entry_size) {
    _set_flag(buffer->data + offset, FLAG_WRAP_AROUND);
    offset = 0;
    wrapped = true;
  }

  EntryHeader *header = (EntryHeader *)(buffer->data + offset);
  _set_flag((uint8_t *)&header->flag, FLAG_NOT_READY);
  header->entry_size = full_entry_size;

  *dest = (void *)(((uint8_t *)header) + sizeof(EntryHeader));

  header->payload_size = size;
  header->seq = tail;

  return ipc_create_transaction(wrapped ? tail + space_to_wrap : tail, IPC_OK);
}

IpcStatus ipc_buffer_commit_entry(IpcBuffer *buffer, const IpcEntryId id) {
  if (buffer == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t offset = RELATIVE(id, buffer->header->data_size);
  EntryHeader *header = (EntryHeader *)(buffer->data + offset);
  _set_flag((uint8_t *)&header->flag, FLAG_READY);

  return IPC_OK;
}

IpcStatus _ipc_buffer_skip(IpcBuffer *buffer, const IpcEntryId id,
                           const bool force) {
  if (buffer == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  uint64_t head;
  EntryHeader *header;
  Flag flag;
  do {
    head = atomic_load(&buffer->header->head);
    if (!force && head != id) {
      return IPC_TRANSACTION_MISS_MATCHED;
    }

    const IpcStatus status = _read_entry_header(buffer, head, &header);
    if (status == IPC_EMPTY) {
      return IPC_EMPTY;
    }

    if (force && status == IPC_LOCKED) {
      break;
    }

    if (status == IPC_LOCKED) {
      return status;
    }

    flag = _read_flag((uint8_t *)header);
  } while (!atomic_compare_exchange_strong(
      &header->flag, &flag,
      FLAG_LOCKED)); // race condition with producer NOT_READY -> READY case

  return atomic_compare_exchange_strong(&buffer->header->head, &head,
                                        head + header->entry_size)
             ? IPC_OK
             : IPC_ERR;
}

IpcStatus _read_entry_header(const IpcBuffer *buffer, const uint64_t head,
                             EntryHeader **dest) {
  if (head == buffer->header->tail &&
      head == atomic_load(&buffer->header->tail)) {
    return IPC_EMPTY;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  const uint64_t relative_head = RELATIVE(head, buffer_size);

  const Flag flag = _read_flag(buffer->data + relative_head);
  EntryHeader *header;
  if (flag == FLAG_WRAP_AROUND) {
    header = (EntryHeader *)buffer->data;
  } else {
    header = (EntryHeader *)(buffer->data + relative_head);
  }

  *dest = header;

  if (flag == FLAG_NOT_READY) {
    return IPC_NOT_READY;
  }

  if (flag == FLAG_LOCKED) {
    return IPC_LOCKED;
  }

  return head != header->seq ? IPC_CORRUPTED : IPC_OK;
}

inline uint64_t _find_max_power_of_2(const uint64_t max) {
  uint64_t res = 1;
  while (res < max) {
    res <<= 1;
  }

  return res == max ? max : (res >> 1);
}

inline IpcTransaction _transaction(const uint64_t id, const IpcStatus status) {
  return (IpcTransaction){.entry_id = id, .status = status};
}

inline Flag _read_flag(const void *addr) {
  const _Atomic Flag *atomic_flag = (_Atomic Flag *)addr;
  return atomic_load_explicit(atomic_flag, memory_order_acquire);
}

inline void _set_flag(void *addr, const Flag flag) {
  _Atomic Flag *atomic_flag = (_Atomic Flag *)addr;
  atomic_store_explicit(atomic_flag, flag, memory_order_release);
}
