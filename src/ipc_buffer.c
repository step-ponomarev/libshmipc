#include "ipc_buffer.h"
#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_u_int64_t.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define FLAG_NOT_READY 0
#define FLAG_READY 1
#define FLAG_TO_START 2

#define BUFFER_SIZE 1024 * 1024 // mb

// TODO: поодержать колльцо..
// TODO: решить проблему минимальног записи в упор. entry_size == buffer size/
// Если tail догнал и перегнал head, то нужно перезаписать head на tail, иначе
// он может прочитать неправильные данные. Либо вообще не давать обгонять head

typedef struct IpcBuffer {
  _Atomic size_t head;
  _Atomic size_t tail;
  char data[BUFFER_SIZE];
} IpcBuffer;

int _open_shm(const char *, const size_t);
IpcBuffer *_open_buffer(const char *, const char);
size_t _reserve_head(IpcBuffer *);
size_t _reserve_tail(IpcBuffer *, const size_t);

IpcBuffer *ipc_create_buffer(const char *name) { return _open_buffer(name, 1); }

IpcBuffer *ipc_open_buffer(const char *name) { return _open_buffer(name, 0); }

void ipc_write(IpcBuffer *buffer, const void *data, const size_t len) {
  const size_t tail = _reserve_tail(buffer, len);
  char *ready_flag_ptr = buffer->data + tail;
  *ready_flag_ptr = FLAG_NOT_READY;

  char *curr_ptr = ready_flag_ptr + sizeof(char);
  memcpy(curr_ptr, &len, sizeof(len));

  curr_ptr += sizeof(len);
  memcpy(curr_ptr, data, len);

  *ready_flag_ptr = FLAG_READY;
}

IpcEntry ipc_read(IpcBuffer *buffer) {
  const size_t head = _reserve_head(buffer);
  const char *ready_flag_ptr = buffer->data + head;
  if (*ready_flag_ptr != FLAG_READY) {
    fprintf(stderr, "Reading not ready entry\n");
    exit(1);
  }

  const char *curr_ptr = ready_flag_ptr + sizeof(char);
  IpcEntry entry;
  memcpy(&entry.size, curr_ptr, sizeof(entry.size));

  entry.payload = malloc(entry.size);

  curr_ptr += sizeof(entry.size);
  memcpy(entry.payload, curr_ptr, entry.size);

  return entry;
}
// TODO: нужно подкопить стамины и пофиксить все гонки и проч. Должно работать
// на минимальном объеме
size_t _reserve_head(IpcBuffer *buffer) {
  size_t prev_head;
  size_t tail;
  size_t new_head;
  size_t to_start;
  size_t fit;
  char flag;
  do {
    prev_head = atomic_load_explicit(&buffer->head, memory_order_acquire);
    tail = atomic_load_explicit(&buffer->tail, memory_order_acquire);
    if (tail == prev_head) {
      continue;
    }

    char *ptr = buffer->data + prev_head;
    to_start = *ptr == FLAG_TO_START;
    if (to_start) {
      ptr = buffer->data;
    }

    flag = *ptr;
    if (flag == FLAG_NOT_READY) {
      continue;
    }

    const size_t *size_ptr = (size_t *)(ptr + sizeof(char));
    new_head = sizeof(char) + sizeof(size_t) + *size_ptr;
    if (!to_start) {
      new_head += prev_head;
    }

    fit = new_head <= BUFFER_SIZE;
    if (to_start && fit) {
      new_head = 0;
    }

  } while (
      prev_head == tail || flag == FLAG_NOT_READY ||
      !atomic_compare_exchange_strong(&buffer->head, &prev_head, new_head));

  if (to_start && !fit) {
    return 0;
  }

  return prev_head;
}

size_t _reserve_tail(IpcBuffer *buffer, const size_t len) {
  const size_t entry_size = len + sizeof(len) + sizeof(char);
  if (entry_size >= BUFFER_SIZE) {

    // Проблема еще хуже, tail дулает круг и у нас полное равенство
    // нужно либо делать пустой байт между записями. либо делать кольцо хитрее
    fprintf(stderr, "Buffer[%d] must be begger than entry : [%d]\n",
            BUFFER_SIZE, entry_size);
    exit(1);
  }

  size_t prev_tail;
  size_t new_tail;
  size_t to_start = 0;
  size_t fit = 0;
  do {
    prev_tail = atomic_load_explicit(&buffer->tail, memory_order_acquire);

    new_tail = prev_tail + entry_size;
    to_start = new_tail >= BUFFER_SIZE;
    fit = new_tail <= BUFFER_SIZE;

    if (to_start) {
      new_tail = fit ? 0 : entry_size;
    }
  } while (
      !atomic_compare_exchange_strong(&buffer->tail, &prev_tail, new_tail));

  if (to_start && !fit) {
    char *to_start_flag_ptr = buffer->data + prev_tail;
    *to_start_flag_ptr = FLAG_TO_START;
    return 0;
  }

  return prev_tail;
}

IpcBuffer *_open_buffer(const char *name, const char trunc) {
  const size_t allocation_size = sizeof(IpcBuffer);
  const int fd = _open_shm(name, allocation_size);
  if (fd < 0) {
    perror("Shared memory segment opening error\n");
    exit(1);
  }

  IpcBuffer *buffer =
      mmap(NULL, allocation_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (trunc) {
    atomic_store_explicit(&buffer->head, 0, memory_order_release);
    atomic_store_explicit(&buffer->tail, 0, memory_order_release);
  }

  return buffer;
}

int _open_shm(const char *name, const size_t size) {
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR);
  if (fd >= 0) {
    if (ftruncate(fd, size) < 0) {
      return -1;
    }

    return fd;
  }

  if (errno != EEXIST) {
    return -1;
  }

  fd = shm_open(name, O_RDWR);
  if (fd < 0) {
    return -1;
  }

  return fd;
}
