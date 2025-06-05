#include "ipc_buffer.h"
#include "ipc_status.h"
#include "ipc_utils.h"
#include "lock/read_write_lock.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_u_int8_t.h>
#include <unistd.h>

#define IPC_DATA_ALIGN 8

typedef uint8_t Flag;

static const Flag FLAG_NOT_READY = 0;
static const Flag FLAG_READY = 1;
static const Flag FLAG_WRAP_AROUND = 2;

typedef uint8_t Lock;

static const Lock UNLOCK = 0;
static const Lock LOCK = 1;
static const Lock UNLOCKING = 2;

typedef struct IpcBufferHeader {
  union {
    uint64_t __align;
    ReadWriteLock read_write_lock;
  };
  _Atomic uint64_t head;
  _Atomic uint64_t tail;
  _Atomic uint64_t locked_tail;
  uint64_t data_size;
} IpcBufferHeader;

typedef struct IpcBuffer {
  const uint64_t data_size;

  // mmaped segment
  IpcBufferHeader *header;
  uint8_t *data;
} IpcBuffer;

typedef struct EntryHeader {
  _Atomic Flag flag;
  uint64_t seq;
  uint32_t payload_size;
  uint32_t entry_size;
} EntryHeader;

uint64_t _find_max_power_of_2(const uint64_t);
void _free_safe(void *);
EntryHeader *_fetch_tail_header(const IpcBuffer *, const uint64_t);

inline uint64_t ipc_optimize_size(uint64_t size) {
  return size + sizeof(IpcBufferHeader);
}

IpcBuffer *ipc_buffer_attach(uint8_t *mem, const uint64_t size) {
  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    fprintf(stderr, "ipc_buffer_attach: malloc is failed\n");
    return NULL;
  }

  IpcBuffer tmp = {.data_size =
                       _find_max_power_of_2(size - sizeof(IpcBufferHeader)),
                   .header = (IpcBufferHeader *)mem,
                   .data = (mem + sizeof(IpcBufferHeader))};
  memcpy(buffer, &tmp, sizeof(IpcBuffer));

  return buffer;
}

IpcStatus ipc_buffer_init(IpcBuffer *buffer) {
  buffer->header->data_size = buffer->data_size;
  if (!rw_init(&buffer->header->read_write_lock)) {
    fprintf(stderr,
            "ipc_buffer_init: read/write lock initialization is failed\n");
    return IPC_ERR;
  }

  atomic_init(&buffer->header->head, 0);
  atomic_init(&buffer->header->tail, 0);

  return IPC_OK;
}

IpcStatus ipc_write(IpcBuffer *buffer, const void *data, const uint64_t size) {
  if (buffer == NULL || data == NULL) {
    fprintf(stderr, "ipc_write: arguments is null\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buffer_size || size == 0) {
    fprintf(stderr, "ipc_write: too large entry\n");
    return IPC_ERR_INVALID_SIZE;
  }

  uint64_t head;
  uint64_t relative_head;
  uint64_t space_to_wrap;
  do {
    head = atomic_load(&buffer->header->head);
    uint64_t tail = atomic_load(&buffer->header->tail);
    relative_head = RELATIVE(head, buffer_size);
    // to move header on start
    space_to_wrap = buffer_size - relative_head;
    if (space_to_wrap < sizeof(EntryHeader)) {
      full_entry_size =
          ALIGN_UP(space_to_wrap + sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
    } else if (space_to_wrap <
               full_entry_size) { // payload will be on position 0
      full_entry_size =
          ALIGN_UP(full_entry_size + (space_to_wrap - sizeof(EntryHeader)),
                   IPC_DATA_ALIGN);
    }

    const uint64_t free_space = buffer_size - (uint64_t)(head - tail);
    if (free_space < full_entry_size) { // no space or payload cannot be
      return IPC_NO_SPACE_CONTIGUOUS;
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + full_entry_size));

  uint64_t offset = relative_head;
  if (space_to_wrap < sizeof(EntryHeader)) {
    _Atomic Flag *atomic_flag = (_Atomic Flag *)(buffer->data + relative_head);
    atomic_store(atomic_flag, FLAG_WRAP_AROUND);
    offset = RELATIVE(offset + space_to_wrap, buffer_size);
  }

  EntryHeader *header = (EntryHeader *)(buffer->data + offset);
  atomic_store(&header->flag, FLAG_NOT_READY);
  header->entry_size = full_entry_size;

  offset = RELATIVE(offset + sizeof(EntryHeader), buffer_size);
  uint8_t *entry_payload_ptr = buffer->data + offset;

  space_to_wrap = buffer_size - offset;
  if (space_to_wrap < size) {
    entry_payload_ptr = buffer->data;
  }

  memcpy(entry_payload_ptr, data, size);
  header->payload_size = size;
  header->seq = head;

  atomic_store(&header->flag, FLAG_READY);
  return IPC_OK;
}

IpcStatus ipc_read(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    fprintf(stderr, "ipc_read: arguments cannot be null\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  if (!rw_read_try_lock(&buffer->header->read_write_lock)) {
    if (lock_erno != LOCK_OK) {
      fprintf(stderr, "ipc_read: read lock acquire is failed\n");
      return IPC_ERR;
    }

    return IPC_BUFFER_LOCKED;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  uint64_t tail;
  uint64_t full_entry_size;
  uint64_t relative_tail;

  IpcEntry entry;
  entry.payload = NULL;
  do {
    uint64_t head = atomic_load(&buffer->header->head);
    tail = atomic_load(&buffer->header->tail);

    EntryHeader *header;
    if (head == tail || (header = _fetch_tail_header(buffer, tail)) == NULL) {
      free(entry.payload);
      if (!rw_unlock(&buffer->header->read_write_lock)) {
        fprintf(stderr, "ipc_read: read lock release is failed\n");
        return IPC_ERR;
      }

      return IPC_EMPTY;
    }

    full_entry_size = header->entry_size;
    relative_tail =
        header->flag == FLAG_WRAP_AROUND ? 0 : RELATIVE(tail, buffer_size);
    uint64_t space_to_wrap =
        buffer_size - (relative_tail + sizeof(EntryHeader));

    if (entry.payload == NULL) {
      entry.payload = malloc(header->payload_size);
    } else if (entry.size != header->payload_size) {
      entry.payload = realloc(entry.payload, header->payload_size);
    }

    if (entry.payload == NULL) {
      perror("ipc_read: allocation is failed\n");
      if (!rw_unlock(&buffer->header->read_write_lock)) {
        fprintf(stderr, "ipc_read: read lock release is failed\n");
        return IPC_ERR;
      }
      return IPC_ERR_ALLOCATION;
    }

    entry.size = header->payload_size;
    relative_tail =
        space_to_wrap < entry.size ? 0 : relative_tail + sizeof(EntryHeader);
    memcpy(entry.payload, buffer->data + relative_tail, entry.size);
  } while (!atomic_compare_exchange_strong(&buffer->header->tail, &tail,
                                           tail + full_entry_size));

  memcpy(dest, &entry, sizeof(entry));

  if (!rw_unlock(&buffer->header->read_write_lock)) {
    fprintf(stderr, "ipc_read: read lock release is failed\n");
    return IPC_ERR;
  }

  return IPC_OK;
}

IpcStatus ipc_read_lock(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    fprintf(stderr, "ipc_read_lock: arguments cannot be null\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  if (!rw_write_lock(&buffer->header->read_write_lock)) {
    fprintf(stderr, "ipc_read_lock: write lock acquire is failed\n");
    return IPC_ERR;
  }

  const uint64_t head = atomic_load(&buffer->header->head);
  const uint64_t tail = atomic_load(&buffer->header->tail);

  EntryHeader *header;
  if (head == tail || ((header = _fetch_tail_header(buffer, tail)) == NULL)) {
    if (!rw_unlock(&buffer->header->read_write_lock)) {
      fprintf(stderr, "ipc_read_lock: write lock release is failed\n");
      return IPC_ERR;
    }
    return IPC_EMPTY;
  }

  dest->payload = (uint8_t *)header + sizeof(EntryHeader);
  dest->size = header->payload_size;

  atomic_store(&buffer->header->locked_tail, tail);

  return IPC_OK;
}

//  tail must be static on method call
IpcStatus ipc_read_release(IpcBuffer *buffer) {
  if (buffer == NULL) {
    fprintf(stderr, "ipc_read_release: arguments cannot be null\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t head = atomic_load(&buffer->header->head);
  const uint64_t tail = atomic_load(&buffer->header->tail);

  EntryHeader *header;
  if (head == tail || ((header = _fetch_tail_header(buffer, tail)) == NULL)) {
    fprintf(stderr, "ipc_read_release: illegal state, no data\n");
    if (!rw_unlock(&buffer->header->read_write_lock)) {
      fprintf(stderr, "ipc_read_release: write lock release is failed\n");
      return IPC_ERR;
    }
    return IPC_WARN;
  }

  uint64_t locked_tail = atomic_load(&buffer->header->locked_tail);
  if (!atomic_compare_exchange_strong(&buffer->header->tail, &locked_tail,
                                      buffer->header->tail +
                                          header->entry_size)) {
    fprintf(stderr, "ipc_read_release: illegal state, tail was changed\n");
    if (!rw_unlock(&buffer->header->read_write_lock)) {
      fprintf(stderr, "ipc_read_release: write lock release is failed\n");
      return IPC_ERR;
    }

    return IPC_ERR;
  }

  if (!rw_unlock(&buffer->header->read_write_lock)) {
    fprintf(stderr, "ipc_read_release: write lock release is failed\n");
    return IPC_ERR;
  }

  return IPC_OK;
}

IpcStatus ipc_peek_unsafe(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    fprintf(stderr, "ipc_peek_unsafe: aruments cannot be NULL\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t head = atomic_load(&buffer->header->head);
  const uint64_t tail = atomic_load(&buffer->header->tail);

  EntryHeader *header;
  if (head == tail || (header = _fetch_tail_header(buffer, tail)) == NULL) {
    return IPC_EMPTY;
  }

  dest->payload = (uint8_t *)header + sizeof(EntryHeader);
  dest->size = header->payload_size;

  return IPC_OK;
}

IpcBufferStat ipc_buffer_stat(const IpcBuffer *buffer) {
  const uint64_t head = atomic_load(&buffer->header->head);
  const uint64_t tail = atomic_load(&buffer->header->tail);

  return (IpcBufferStat){.buffer_size = buffer->data_size,
                         .head_pos = RELATIVE(head, buffer->data_size),
                         .tail_pos = RELATIVE(tail, buffer->data_size)};
}

EntryHeader *_fetch_tail_header(const IpcBuffer *buffer, const uint64_t tail) {
  const uint64_t buffer_size = buffer->header->data_size;
  const uint64_t relative_tail = RELATIVE(tail, buffer_size);
  _Atomic Flag *atomic_flag = (_Atomic Flag *)(buffer->data + relative_tail);
  const Flag flag = atomic_load(atomic_flag);
  if (flag == FLAG_NOT_READY) {
    return NULL;
  }

  EntryHeader *header;
  // need check seq
  if (flag == FLAG_WRAP_AROUND) {
    header = (EntryHeader *)buffer->data;
  } else {
    header = (EntryHeader *)(buffer->data + relative_tail);
  }

  if (tail != header->seq) {
    return NULL;
  }

  return header;
}

inline uint64_t _find_max_power_of_2(const uint64_t max) {
  uint64_t rounder = 1;
  while (rounder < max) {
    rounder <<= 1;
  }

  return rounder == max ? max : rounder >> 1;
}
