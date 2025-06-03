#include "ipc_buffer.h"
#include "ipc_utils.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPC_DATA_ALIGN 8

typedef uint8_t Flag;

static const Flag FLAG_NOT_READY = 0;
static const Flag FLAG_READY = 1;
static const Flag FLAG_WRAP_AROUND = 2;

typedef struct IpcBufferHeader {
  _Atomic uint64_t head;
  _Atomic uint64_t tail;
  uint64_t data_size;
} IpcBufferHeader;

typedef struct IpcBuffer {
  uint64_t data_size;

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

IpcBuffer *ipc_buffer_attach(uint8_t *mem, const uint64_t size) {
  IpcBuffer *buffer = malloc(sizeof(IpcBuffer));
  if (buffer == NULL) {
    fprintf(stderr, "ipc_buffer_attach: malloc is failed\n");
    exit(EXIT_FAILURE);
  }

  buffer->data_size = _find_max_power_of_2(size - sizeof(IpcBufferHeader));
  buffer->header = (IpcBufferHeader *)mem;
  buffer->data = (mem + sizeof(IpcBufferHeader));

  return buffer;
}

IpcStatus ipc_buffer_init(IpcBuffer *buffer) {
  buffer->header->data_size = buffer->data_size;
  buffer->header->head = 0;
  atomic_store_explicit(&buffer->header->tail, 0, memory_order_release);

  return IPC_OK;
}

char ipc_write(IpcBuffer *buffer, const void *data, const uint64_t size) {
  const uint64_t buffer_size = buffer->header->data_size;
  uint64_t full_entry_size =
      ALIGN_UP(sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
  if (full_entry_size > buffer_size) {
    fprintf(stderr, "ipc_write: too large entry\n");
    exit(EXIT_FAILURE);
  }

  uint64_t head;
  uint64_t tail;
  uint64_t relative_head;
  uint64_t space_to_wrap;
  do {
    head = atomic_load_explicit(&buffer->header->head, memory_order_acquire);
    tail = atomic_load_explicit(&buffer->header->tail, memory_order_acquire);

    relative_head = RELATIVE(head, buffer_size);
    // to move header on start
    space_to_wrap = buffer_size - relative_head;
    if (space_to_wrap < sizeof(EntryHeader)) {
      full_entry_size =
          ALIGN_UP(space_to_wrap + sizeof(EntryHeader) + size, IPC_DATA_ALIGN);
    }

    const uint64_t free_space = buffer_size - (uint64_t)(head - tail);
    if (free_space < full_entry_size) {
      return 0;
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->head, &head,
                                           head + full_entry_size));

  uint64_t offset = relative_head;
  if (space_to_wrap < sizeof(EntryHeader)) {
    _Atomic Flag *atomic_flag = (_Atomic Flag *)(buffer->data + relative_head);
    atomic_store_explicit(atomic_flag, FLAG_WRAP_AROUND, memory_order_release);
    offset = RELATIVE(offset + space_to_wrap, buffer_size);
  }

  EntryHeader *header = (EntryHeader *)(buffer->data + offset);
  atomic_store_explicit(&header->flag, FLAG_NOT_READY, memory_order_release);
  header->entry_size = full_entry_size;

  offset = RELATIVE(offset + sizeof(EntryHeader), buffer_size);
  uint8_t *entry_payload_ptr = buffer->data + offset;

  space_to_wrap = buffer_size - offset;
  if (space_to_wrap == 0) {
    offset = 0;
    space_to_wrap = buffer_size;
  }

  if (space_to_wrap >= size) {
    memcpy(entry_payload_ptr, data, size);
  } else {
    memcpy(entry_payload_ptr, data, space_to_wrap);
    memcpy(buffer->data, ((uint8_t *)data) + space_to_wrap,
           size - space_to_wrap);
  }
  header->payload_size = size;
  header->seq = head;

  atomic_store_explicit(&header->flag, FLAG_READY, memory_order_release);
  return 1;
}

IpcEntry ipc_read(IpcBuffer *buffer) {
  const uint64_t buffer_size = buffer->header->data_size;

  uint64_t tail;
  uint64_t full_entry_size;
  uint64_t relative_tail;
  Flag flag;

  IpcEntry entry;
  entry.payload = NULL;
  do {
    if (entry.payload != NULL) {
      free(entry.payload);
      entry.payload = NULL;
    }

    uint64_t head =
        atomic_load_explicit(&buffer->header->head, memory_order_acquire);
    tail = atomic_load_explicit(&buffer->header->tail, memory_order_acquire);
    if (head == tail) {
      return ((IpcEntry){.size = 0, .payload = NULL});
    }

    relative_tail = RELATIVE(tail, buffer_size);
    _Atomic Flag *atomic_flag = (_Atomic Flag *)(buffer->data + relative_tail);
    flag = atomic_load_explicit(atomic_flag, memory_order_acquire);
    if (flag == FLAG_NOT_READY) {
      return ((IpcEntry){.size = 0, .payload = NULL});
    }

    EntryHeader *header;
    // need check seq
    if (flag == FLAG_WRAP_AROUND) {
      header = (EntryHeader *)buffer->data;
    } else {
      header = (EntryHeader *)(buffer->data + relative_tail);
    }

    if (tail != header->seq) {
      return ((IpcEntry){.size = 0, .payload = NULL});
    }

    full_entry_size = header->entry_size;

    relative_tail = flag == FLAG_WRAP_AROUND ? 0 : RELATIVE(tail, buffer_size);
    uint64_t space_to_wrap =
        buffer_size - (relative_tail + sizeof(EntryHeader));

    entry.size = header->payload_size;
    entry.payload = malloc(entry.size);
    if (entry.payload == NULL) {
      perror("ipc_read: mallo is failed\n");
      exit(EXIT_FAILURE);
    }

    if (space_to_wrap >= entry.size) {
      memcpy(entry.payload, buffer->data + relative_tail + sizeof(EntryHeader),
             entry.size);
    } else {
      memcpy(entry.payload, buffer->data + relative_tail + sizeof(EntryHeader),
             space_to_wrap);

      uint64_t diff = entry.size - space_to_wrap;
      memcpy(entry.payload + space_to_wrap, buffer->data + diff, diff);
    }

  } while (!atomic_compare_exchange_strong(&buffer->header->tail, &tail,
                                           tail + full_entry_size));

  return entry;
}

uint64_t _find_max_power_of_2(const uint64_t max) {
  uint64_t rounder = 1;
  while (rounder < max) {
    rounder <<= 1;
  }

  return rounder == max ? max : rounder >> 1;
}
