#include "ipc_utils.h"
#include "shmipc/ipc_common.h"
#include <errno.h>
#include <fcntl.h>
#include <shmipc/ipc_mmap.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define OPEN_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP

IntegerResult _open_shm(const char *, const uint64_t);
IpcStatusResult _unmap(void *, const uint64_t);
IpcStatusResult _unlink(const char *);
StringResult _copy(const char *);

IpcMemorySegmentResult ipc_mmap(const char *name, const uint64_t size) {
  if (name == NULL) {
    return IpcMemorySegmentResult_error(IPC_ERR_INVALID_ARGUMENT,
                                        "invalid argument: name is NULL");
  }

  if (size == 0) {
    return IpcMemorySegmentResult_error(IPC_ERR_INVALID_ARGUMENT,
                                        "invalid argument: size == 0");
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    return IpcMemorySegmentResult_error(
        IPC_ERR_SYSTEM,
        "system error: sysconf(_SC_PAGESIZE) is failed, check errno");
  }

  const uint64_t aligned_size = ALIGN_UP(size, page_size);
  const IntegerResult fd_result = _open_shm(name, aligned_size);
  if (IntegerResult_is_error(fd_result)) {
    return IpcMemorySegmentResult_error(fd_result.ipc_status,
                                        fd_result.error_detail);
  }

  uint8_t *mmapedped = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd_result.result, 0);
  close(fd_result.result);
  if (mmapedped == MAP_FAILED) {
    return IpcMemorySegmentResult_error(IPC_ERR_SYSTEM,
                                        "system error: mmap is failed");
  }

  IpcMemorySegment *memory_segment = malloc(sizeof(IpcMemorySegment));
  if (memory_segment == NULL) {
    _unmap(mmapedped, aligned_size);
    return IpcMemorySegmentResult_error(
        IPC_ERR_SYSTEM, "system error: memory segment allocation is failed");
  }

  const StringResult name_copy = _copy(name);
  if (StringResult_is_error(name_copy)) {
    _unmap(mmapedped, aligned_size);
    free(memory_segment);
    return IpcMemorySegmentResult_error(IPC_ERR_SYSTEM, name_copy.error_detail);
  }

  memory_segment->name = name_copy.result;
  memory_segment->memory = mmapedped;
  memory_segment->size = aligned_size;

  return IpcMemorySegmentResult_ok(IPC_OK, memory_segment);
}

IpcStatusResult ipc_unmap(IpcMemorySegment *segment) {
  if (segment == NULL) {
    return IpcStatusResult_error(IPC_ERR_INVALID_ARGUMENT,
                                 "invalid argument: segment is NULL");
  }

  if (segment->memory == NULL) {
    return IpcStatusResult_error(IPC_ERR_ILLEGAL_STATE,
                                 "illegal state: segment->memory is NULL");
  }

  const IpcStatusResult unmap_result = _unmap(segment->memory, segment->size);
  if (IpcStatusResult_is_error(unmap_result)) {
    return unmap_result;
  }

  free(segment->name);
  free(segment);

  return unmap_result;
}

IpcStatusResult ipc_unlink(IpcMemorySegment *segment) {
  if (segment == NULL) {
    return IpcStatusResult_error(IPC_ERR_INVALID_ARGUMENT,
                                 "invalid argument: segment is NULL");
  }

  if (segment->memory == NULL) {
    return IpcStatusResult_error(IPC_ERR_ILLEGAL_STATE,
                                 "illegal state: segment->memory is NULL");
  }

  const IpcStatusResult unlink_result = _unlink(segment->name);
  if (IpcStatusResult_is_error(unlink_result)) {
    return unlink_result;
  }

  return ipc_unmap(segment);
}

IpcStatusResult _unmap(void *memory, const uint64_t size) {
  if (munmap(memory, size) != 0) {
    return IpcStatusResult_error(IPC_ERR_SYSTEM,
                                 "system error: munmap is failed");
  }

  return IpcStatusResult_ok(IPC_OK, IPC_OK);
}

IpcStatusResult _unlink(const char *name) {
  if (shm_unlink(name) != 0) {
    return IpcStatusResult_error(IPC_ERR_SYSTEM,
                                 "system error: shm_unlink is failed");
  }

  return IpcStatusResult_ok(IPC_OK, IPC_OK);
}

IntegerResult _open_shm(const char *name, const uint64_t size) {
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, OPEN_MODE);
  if (fd >= 0) {
    if (ftruncate(fd, size) < 0) {
      return IntegerResult_error(IPC_ERR_SYSTEM,
                                 "system error: ftruncate is failed");
    }

    return IntegerResult_ok(IPC_OK, fd);
  }

  if (errno != EEXIST) {
    return IntegerResult_error(
        IPC_ERR_SYSTEM, "system error: unexpected shm_open error, check errno");
  }

  fd = shm_open(name, O_RDWR, OPEN_MODE);
  if (fd < 0) {
    return IntegerResult_error(
        IPC_ERR_SYSTEM, "system error: unexpected shm_open error, check errno");
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return IntegerResult_error(IPC_ERR_SYSTEM, "system error: fstat is failed");
  }

  if ((uint64_t)st.st_size != size) {
    close(fd);
    return IntegerResult_error(IPC_ERR_ILLEGAL_STATE,
                               "illegal state: unexpected file size");
  }

  return IntegerResult_ok(IPC_OK, fd);
}

StringResult _copy(const char *src) {
  const size_t len = strlen(src) + 1;
  char *cpy = malloc(len);
  if (cpy == NULL) {
    return StringResult_error(IPC_ERR_SYSTEM,
                              "system error: char buffer allocation is failed");
  }

  memcpy(cpy, src, len);

  return StringResult_ok(IPC_OK, cpy);
}
