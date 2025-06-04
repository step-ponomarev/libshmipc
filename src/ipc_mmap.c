#include "ipc_mmap.h"
#include "ipc_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int _open_shm(const char *, const uint64_t);
IpcStatus _unmap(void *, const uint64_t);

IpcMemorySegment *ipc_mmap(const char *name, const uint64_t size) {
  if (name == NULL || size == 0) {
    fprintf(stderr, "ipc_mmap: invalid arguments\n");
    return NULL;
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  const uint64_t aligned_size = ALIGN_UP(size, page_size);
  const int fd = _open_shm(name, aligned_size);
  if (fd < 0) {
    perror("ipc_mmap: Shared memory oppening is failed\n");
    return NULL;
  }

  uint8_t *memory =
      mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (close(fd) != 0) {
    perror("ipc_mmap: Close descriptor is failed\n");
    return NULL;
  }

  if (memory == MAP_FAILED) {
    perror("ipc_mmap: Mmap is failed\n");
    return NULL;
  }

  const IpcMemorySegment seg = {
      .name = name, .size = aligned_size, .memory = memory};

  IpcMemorySegment *res = malloc(sizeof(IpcMemorySegment));
  memcpy(res, &seg, sizeof(IpcMemorySegment));

  return res;
}

IpcStatus ipc_unmmap(IpcMemorySegment segment) {
  if (segment.memory == NULL) {
    fprintf(stderr, "ipc_unmmap: Segment is not mapped\n");
    return IPC_ERR_INVALID_ARGUMENT;
  }

  return _unmap(segment.memory, segment.size);
}

IpcStatus ipc_unlink(const IpcMemorySegment segment) {
  if (shm_unlink(segment.name) != 0) {
    perror("ipc_destroy_shared_segment: shm_unlink is failed\n");
    return IPC_ERR;
  }

  return IPC_OK;
}

IpcStatus _unmap(void *memory, const uint64_t size) {
  if (munmap(memory, size) != 0) {
    perror("ipc_unmmap: unmmap is failed\n");
    return IPC_ERR;
  }

  return IPC_OK;
}

int _open_shm(const char *name, const uint64_t size) {
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
