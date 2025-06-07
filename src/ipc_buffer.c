#include "ipc_buffer.h"
#include "ipc_status.h"
#include "ipc_utils.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO: Optimize memory barier using

#define IPC_DATA_ALIGN 8

typedef uint8_t Flag;
static const Flag FLAG_NOT_READY = 0;
static const Flag FLAG_READY = 1;
static const Flag FLAG_WRAP_AROUND = 2;

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
void _free_safe(void *);
IpcStatus _fetch_entry_header(const IpcBuffer *, const uint64_t,
                              EntryHeader **);

inline uint64_t ipc_allign_size(uint64_t size) {
  return size + sizeof(IpcBufferHeader);
}

IpcBuffer *ipc_buffer_create(uint8_t *mem, const uint64_t size) {
  if (mem == NULL || size <= sizeof(IpcBufferHeader) ||
      size - sizeof(IpcBufferHeader) < 2) {
    return NULL;
  }

  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    return NULL;
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = mem + sizeof(IpcBufferHeader);

  atomic_init(&buffer->header->data_size,
              _find_max_power_of_2(size - sizeof(IpcBufferHeader)));
  atomic_init(&buffer->header->head, 0);
  atomic_init(&buffer->header->tail, 0);

  return buffer;
}

IpcBuffer *ipc_buffer_attach(uint8_t *mem) {
  if (mem == NULL) {
    return NULL;
  }

  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    return NULL;
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = mem + sizeof(IpcBufferHeader);

  return buffer;
}

IpcStatus ipc_write(IpcBuffer *buffer, const void *data, const uint64_t size) {
  if (buffer == NULL || data == NULL || size == 0) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  uint8_t *payload;
  IpcStatus status = ipc_reserve_entry(buffer, size, &payload);
  if (status != IPC_OK) {
    return status;
  }

  memcpy(payload, data, size);
  status = ipc_commit_entry(buffer, payload);

  return status;
}

IpcStatus ipc_read(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  const uint64_t dest_size = dest->size;
  uint64_t head;
  uint64_t full_entry_size;

  do {
    head = atomic_load(&buffer->header->head);

    EntryHeader *header;
    const IpcStatus status = _fetch_entry_header(buffer, head, &header);
    if (status != IPC_OK) {
      return status;
    }

    full_entry_size = header->entry_size;
    dest->size = header->payload_size;
    if (dest_size < dest->size) {
      return IPC_ERR_TOO_SMALL;
    }

    uint8_t *payload = ((uint8_t *)header) + sizeof(EntryHeader);
    memcpy(dest->payload, payload, header->payload_size);
  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + full_entry_size));

  return IPC_OK;
}

IpcStatus ipc_delete(IpcBuffer *buffer) {
  if (buffer == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  uint64_t head;
  EntryHeader *header;
  do {
    head = atomic_load(&buffer->header->head);
    if (_fetch_entry_header(buffer, head, &header) == IPC_EMPTY) {
      return IPC_EMPTY;
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + header->entry_size));

  return IPC_OK;
}

IpcStatus ipc_peek(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t head = atomic_load(&buffer->header->head);
  IpcStatus status;
  EntryHeader *header;
  if (_fetch_entry_header(buffer, head, &header) != IPC_OK) {
    return status;
  }

  dest->size = header->payload_size;
  dest->payload = ((uint8_t *)header) + sizeof(EntryHeader);

  return IPC_OK;
}

IpcStatus ipc_reserve_entry(IpcBuffer *buffer, const uint64_t size,
                            uint8_t **dest) {
  if (buffer == NULL || size == 0) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buffer_size) {
    return IPC_ERR_INVALID_SIZE;
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
        return IPC_NO_SPACE_CONTIGUOUS;
      }
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->tail, &tail,
                                           tail + full_entry_size));

  uint64_t offset = relative_tail;
  if (space_to_wrap < full_entry_size) {
    _Atomic Flag *atomic_flag = (_Atomic Flag *)(buffer->data + offset);
    atomic_store(atomic_flag, FLAG_WRAP_AROUND);
    offset = 0;
  } else {
    offset = relative_tail;
  }

  EntryHeader *header = (EntryHeader *)(buffer->data + offset);
  atomic_store(&header->flag, FLAG_NOT_READY);
  header->entry_size = full_entry_size;

  *dest = ((uint8_t *)header) + sizeof(EntryHeader);

  header->payload_size = size;
  header->seq = tail;

  return IPC_OK;
}

IpcStatus ipc_commit_entry(IpcBuffer *buffer, const uint8_t *payload) {
  if (buffer == NULL || payload == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  EntryHeader *header = (EntryHeader *)(payload - sizeof(EntryHeader));
  atomic_store(&header->flag, FLAG_READY);

  return IPC_OK;
}

IpcStatus _fetch_entry_header(const IpcBuffer *buffer, const uint64_t head,
                              EntryHeader **dest) {
  if (head == buffer->header->tail &&
      head == atomic_load(&buffer->header->tail)) {
    return IPC_EMPTY;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  const uint64_t relative_head = RELATIVE(head, buffer_size);

  const _Atomic Flag *atomic_flag =
      (_Atomic Flag *)(buffer->data + relative_head);
  const Flag flag = atomic_load(atomic_flag);
  if (flag == FLAG_NOT_READY) {
    return IPC_NOT_READY;
  }

  EntryHeader *header;
  if (flag == FLAG_WRAP_AROUND) {
    header = (EntryHeader *)buffer->data;
  } else {
    header = (EntryHeader *)(buffer->data + relative_head);
  }

  *dest = header;

  return head != header->seq ? IPC_CORRUPTED : IPC_OK;
}

inline uint64_t _find_max_power_of_2(const uint64_t max) {
  uint64_t res = 1;
  while (res < max) {
    res <<= 1;
  }

  return res == max ? max : (res >> 1);
}
