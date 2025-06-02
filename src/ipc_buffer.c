#include "ipc_buffer.h"
#include <_stdlib.h>
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

#define BUFFER_SIZE 1024 * 1024 // 1 gb

typedef struct IpcBuffer {
  _Atomic size_t head;
  _Atomic size_t tail;
  char data[BUFFER_SIZE];
} IpcBuffer;

int open_shm(const char *name, const size_t size) {
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

IpcBuffer *ipc_create_buffer(const char *name) {
  // TODO: chech name is not null and >= 1
  // also permissions
  const int fd = open_shm(name, sizeof(IpcBuffer));
  if (fd < 0) {
    perror("Shared memory segment opening error\n");
    exit(1);
  }

  IpcBuffer *buffer =
      mmap(NULL, sizeof(IpcBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  atomic_store_explicit(&buffer->head, 0, memory_order_release);
  atomic_store_explicit(&buffer->tail, 0, memory_order_release);

  return buffer;
}

void ipc_write(IpcBuffer *buffer, const void *data, const size_t len) {
  const size_t tail = atomic_load_explicit(&buffer->tail, memory_order_acquire);

  char *curr_ptr = buffer->data + tail;
  memcpy(curr_ptr, &len, sizeof(size_t));

  curr_ptr += sizeof(size_t);
  memcpy(curr_ptr, data, len);

  atomic_store_explicit(&buffer->tail, tail + len + sizeof(size_t),
                        memory_order_release);
}

IpcEntry ipc_read(IpcBuffer *buffer) {
  const size_t head = atomic_load_explicit(&buffer->head, memory_order_acquire);
  const size_t tail = atomic_load_explicit(&buffer->tail, memory_order_acquire);
  if (head == tail) {
    return (IpcEntry){.payload = NULL, .size = 0};
  }

  // TODO: Либо можно не копировать даже, а делать ссылку на SHM с пометкой не
  // удлять. Но тут пробелмы с кольцевым буффером будут, когда tail добежит до
  // head, но пока что делаем так, MVP
  const char *curr_ptr = buffer->data + head;

  IpcEntry entry;
  memcpy(&entry.size, curr_ptr, sizeof(entry.size));

  entry.payload = malloc(entry.size);

  curr_ptr += sizeof(entry.size);
  memcpy(entry.payload, curr_ptr, entry.size);

  atomic_store_explicit(&buffer->head, head + entry.size + sizeof(entry.size),
                        memory_order_release);

  return entry;
}

char ipc_has_message(const IpcBuffer *buffer) {
  return atomic_load_explicit(&buffer->head, memory_order_acquire) <
         atomic_load_explicit(&buffer->tail, memory_order_acquire);
}
