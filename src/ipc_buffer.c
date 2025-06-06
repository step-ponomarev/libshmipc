#include "ipc_buffer.h"
#include "ipc_status.h"
#include "ipc_utils.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#define IPC_DATA_ALIGN 8

typedef uint8_t Flag;
// TODO:optimize tail cashing when moving head
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
EntryHeader *_fetch_entry_header(const IpcBuffer *, const uint64_t);

inline uint64_t ipc_allign_size(uint64_t size) {
  return size + sizeof(IpcBufferHeader);
}

IpcBuffer *ipc_buffer_create(uint8_t *mem, const uint64_t size) {
  if (mem == NULL) {
    fprintf(stderr, "ipc_buffer_create: argument is null\n");
    return NULL;
  }

  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    perror("ipc_buffer_create: allocation is failed\n");
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
    fprintf(stderr, "ipc_buffer_attach: argument is null\n");
    return NULL;
  }

  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    perror("ipc_buffer_attach: allocation is failed\n");
    return NULL;
  }

  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = mem + sizeof(IpcBufferHeader);

  printf("Attched head: %lld tail: %lld, data_size: %lld\n",
         buffer->header->head, buffer->header->tail, buffer->header->data_size);

  return buffer;
}

// TODO: need fix bugs read/write
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

  uint64_t offset;
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

  uint8_t *entry_payload_ptr = ((uint8_t *)header) + sizeof(EntryHeader);
  memcpy(entry_payload_ptr, data, size);

  header->payload_size = size;
  header->seq = tail;
  atomic_store(&header->flag, FLAG_READY);

  printf("Write entry size: %lld, full entry size: %lld, offset: %lld, tail: "
         "%lld\n",
         size, full_entry_size, offset, tail);

  return IPC_OK;
}

IpcStatus ipc_read(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    fprintf(stderr, "ipc_read: arguments cannot be null\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t buffer_size = buffer->header->data_size;
  uint64_t head;
  uint64_t full_entry_size;

  IpcEntry entry;
  entry.payload = NULL;
  do {
    head = atomic_load(&buffer->header->head);

    EntryHeader *header; // if wrap around -no header
    if ((buffer->header->tail == head &&
         atomic_load(&buffer->header->tail) == head) ||
        (header = _fetch_entry_header(buffer, head)) == NULL) {
      free(entry.payload);
      return IPC_EMPTY;
    }

    if (entry.payload == NULL) {
      entry.payload = malloc(header->payload_size);
    } else if (entry.size != header->payload_size) {
      free(entry.payload);
      entry.payload = malloc(header->payload_size);
    }

    if (entry.payload == NULL) {
      perror("ipc_read: allocation is failed\n");
      return IPC_ERR_ALLOCATION;
    }

    full_entry_size = header->entry_size;
    entry.size = header->payload_size;

    uint8_t *payload = ((uint8_t *)header) + sizeof(EntryHeader);
    memcpy(entry.payload, payload, entry.size);
  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + full_entry_size));

  printf("Readentry size: %lld, full entry size: %lld, head: "
         "%lld\n",
         entry.size, full_entry_size, head);

  memcpy(dest, &entry, sizeof(entry));

  return IPC_OK;
}

IpcStatus ipc_delete(IpcBuffer *buffer) {
  if (buffer == NULL) {
    fprintf(stderr, "ipc_read: arguments cannot be null\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  uint64_t head;
  EntryHeader *header;
  do {
    uint64_t tail = atomic_load(&buffer->header->tail);
    head = atomic_load(&buffer->header->head);

    if (tail == head || (header = _fetch_entry_header(buffer, head)) == NULL) {
      return IPC_EMPTY;
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + header->entry_size));

  return IPC_OK;
}

IpcStatus ipc_peek(IpcBuffer *buffer, IpcEntry *dest) {
  if (buffer == NULL || dest == NULL) {
    fprintf(stderr, "ipc_peek_unsafe: aruments cannot be NULL\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  const uint64_t tail = atomic_load(&buffer->header->tail);
  const uint64_t head = atomic_load(&buffer->header->head);
  EntryHeader *header;
  if (tail == head || (header = _fetch_entry_header(buffer, head)) == NULL) {
    return IPC_EMPTY;
  }

  dest->size = header->payload_size;
  dest->payload =
      (RELATIVE(head, buffer->header->data_size) + header->entry_size >
       buffer->header->data_size)
          ? buffer->data
          : (uint8_t *)header + sizeof(EntryHeader);

  return IPC_OK;
}

EntryHeader *_fetch_entry_header(const IpcBuffer *buffer, const uint64_t addr) {
  const uint64_t buffer_size = buffer->header->data_size;
  const uint64_t relative_head = RELATIVE(addr, buffer_size);

  const _Atomic Flag *atomic_flag =
      (_Atomic Flag *)(buffer->data + relative_head);
  const Flag flag = atomic_load(atomic_flag);
  if (flag == FLAG_NOT_READY) {
    return NULL;
  }

  EntryHeader *header;
  // need check seq
  if (flag == FLAG_WRAP_AROUND) {
    header = (EntryHeader *)buffer->data;
  } else {
    header = (EntryHeader *)(buffer->data + relative_head);
  }

  if (addr != header->seq) {
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
