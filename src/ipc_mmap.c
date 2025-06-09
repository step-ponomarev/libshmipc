#include "ipc_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <shmipc/ipc_mmap.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// TODO: Fix it, use groups and owner
#define OPEN_MODE 0666

int _open_shm(const char *, const uint64_t);
IpcStatus _unmap(void *, const uint64_t);
IpcStatus _unlink(const char *);
char *_copy(const char *);

IpcMemorySegment *ipc_mmap(const char *name, const uint64_t size) {
  if (name == NULL || size == 0) {
    return NULL;
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  const uint64_t aligned_size = ALIGN_UP(size, page_size);
  const int fd = _open_shm(name, aligned_size);
  if (fd < 0) {
    return NULL;
  }

  uint8_t *mem =
      mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (mem == MAP_FAILED) {
    close(fd);
    return NULL;
  }

  close(fd);

  IpcMemorySegment *res = malloc(sizeof(IpcMemorySegment));
  if (res == NULL) {
    _unmap(mem, aligned_size);
    return NULL;
  }

  char *name_copy = _copy(name);
  if (name_copy == NULL) {
    _unmap(mem, aligned_size);
    free(res);
    return NULL;
  }

  res->name = name_copy;
  res->memory = mem;
  res->size = aligned_size;

  return res;
}

IpcStatus ipc_unmap(IpcMemorySegment *segment) {
  if (segment == NULL || segment->memory == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  IpcStatus status = _unmap(segment->memory, segment->size);
  if (status != IPC_OK) {
    return status;
  }

  free(segment->name);
  free(segment);

  return status;
}

IpcStatus ipc_unlink(const IpcMemorySegment *segment) {
  if (segment == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  return _unlink(segment->name);
}

IpcStatus ipc_reset(const char *name) {
  if (name == NULL) {
    return IPC_ERR_INVALID_ARGUMENT;
  }

  return _unlink(name);
}

IpcStatus _unmap(void *memory, const uint64_t size) {
  if (munmap(memory, size) != 0) {
    return IPC_ERR;
  }

  return IPC_OK;
}

IpcStatus _unlink(const char *name) {
  if (shm_unlink(name) != 0) {
    return IPC_ERR;
  }

  return IPC_OK;
}

int _open_shm(const char *name, const uint64_t size) {
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, OPEN_MODE);
  if (fd >= 0) {
    if (ftruncate(fd, size) < 0) {
      return -1;
    }

    return fd;
  }

  if (errno != EEXIST) {
    return -1;
  }

  fd = shm_open(name, O_RDWR, OPEN_MODE);
  if (fd < 0) {
    return -1;
  }

  return fd;
}

char *_copy(const char *src) {
  const size_t len = strlen(src) + 1;
  char *cpy = malloc(len);
  if (cpy == NULL) {
    return NULL;
  }

  memcpy(cpy, src, len);

  return cpy;
}
