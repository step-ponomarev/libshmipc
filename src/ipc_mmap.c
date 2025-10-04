#include "ipc_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <shmipc/ipc_mmap.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define OPEN_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

SHMIPC_API IpcMemorySegmentResult ipc_mmap(const char *name,
                                           const uint64_t size) {
  IpcMmapError error = {.name = NULL,
                        .requested_size = size,
                        .aligned_size = 0,
                        .page_size = 0,
                        .existing_size = 0,
                        .existed = false,
                        .sys_errno = 0};

  if (name == NULL) {
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: name is NULL", error);
  }

  if (size == 0) {
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: size == 0", error);
  }

  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    error.page_size = -1;
    error.sys_errno = errno;
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: sysconf(_SC_PAGESIZE) failed", error);
  }

  const uint64_t aligned_size = ALIGN_UP(size, (uint64_t)page_size);
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, OPEN_MODE);
  bool existed = false;
  if (fd >= 0) {
    if (ftruncate(fd, (off_t)aligned_size) < 0) {
      close(fd);
      error.name = name;
      error.requested_size = size;
      error.aligned_size = aligned_size;
      error.page_size = page_size;
      error.existing_size = 0;
      error.existed = false;
      error.sys_errno = errno;
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: ftruncate failed", error);
    }
  } else {
    if (errno != EEXIST) {
      error.name = name;
      error.requested_size = size;
      error.aligned_size = aligned_size;
      error.page_size = page_size;
      error.existing_size = 0;
      error.existed = false;
      error.sys_errno = errno;
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: shm_open (create) failed", error);
    }

    existed = true;
    errno = 0;
    fd = shm_open(name, O_RDWR, OPEN_MODE);
    if (fd < 0) {
      error.name = name;
      error.requested_size = size;
      error.aligned_size = aligned_size;
      error.page_size = page_size;
      error.existing_size = 0;
      error.existed = existed;
      error.sys_errno = errno;
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: shm_open (open) failed", error);
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
      close(fd);
      error.name = name;
      error.requested_size = size;
      error.aligned_size = aligned_size;
      error.page_size = page_size;
      error.existing_size = 0;
      error.existed = existed;
      error.sys_errno = errno;
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: fstat failed", error);
    }

    if ((uint64_t)st.st_size != aligned_size) {
      close(fd);
      error.name = name;
      error.requested_size = size;
      error.aligned_size = aligned_size;
      error.page_size = page_size;
      error.existing_size = (uint64_t)st.st_size;
      error.existed = existed;
      error.sys_errno = errno;
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_ILLEGAL_STATE,
          "illegal state: existing segment size != aligned size", error);
    }
  }

  void *mapped = mmap(NULL, (size_t)aligned_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
  close(fd);
  if (mapped == MAP_FAILED) {
    error.name = name;
    error.requested_size = size;
    error.aligned_size = aligned_size;
    error.page_size = page_size;
    error.existing_size = 0;
    error.existed = existed;
    error.sys_errno = errno;
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: mmap failed", error);
  }

  IpcMemorySegment *segment = (IpcMemorySegment *)malloc(sizeof(*segment));
  if (segment == NULL) {
    munmap(mapped, (size_t)aligned_size);
    error.name = name;
    error.requested_size = size;
    error.aligned_size = aligned_size;
    error.page_size = page_size;
    error.existing_size = 0;
    error.existed = existed;
    error.sys_errno = errno;
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: memory allocation failed", error);
  }

  const size_t nlen = strlen(name) + 1;
  char *name_copy = (char *)malloc(nlen);
  if (name_copy == NULL) {
    munmap(mapped, (size_t)aligned_size);
    free(segment);
    error.name = name;
    error.requested_size = size;
    error.aligned_size = aligned_size;
    error.page_size = page_size;
    error.existing_size = 0;
    error.existed = existed;
    error.sys_errno = errno;
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: memory allocation failed", error);
  }
  memcpy(name_copy, name, nlen);

  segment->name = name_copy;
  segment->memory = mapped;
  segment->size = aligned_size;

  return IpcMemorySegmentResult_ok(IPC_OK, segment);
}

SHMIPC_API IpcMmapUnmapResult ipc_unmap(IpcMemorySegment *segment) {
  if (segment == NULL) {
    IpcMmapUnmapError body = {.name = NULL, .size = 0, .sys_errno = 0};
    return IpcMmapUnmapResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: segment is NULL", body);
  }

  if (segment->memory == NULL) {
    return IpcMmapUnmapResult_error(IPC_ERR_ILLEGAL_STATE,
                                    "illegal state: segment->memory is NULL");
  }

  errno = 0;
  if (munmap(segment->memory, (size_t)segment->size) != 0) {
    IpcMmapUnmapError body = {
        .name = segment->name, .size = segment->size, .sys_errno = errno};
    return IpcMmapUnmapResult_error_body(IPC_ERR_SYSTEM,
                                         "system error: munmap failed", body);
  }

  free(segment->name);
  free(segment);
  return IpcMmapUnmapResult_ok(IPC_OK);
}

SHMIPC_API IpcMmapUnlinkResult ipc_unlink(IpcMemorySegment *segment) {
  if (segment == NULL) {
    return IpcMmapUnlinkResult_error(IPC_ERR_INVALID_ARGUMENT,
                                     "invalid argument: segment is NULL");
  }

  IpcMmapUnlinkError error = {.name = segment->name};
  if (shm_unlink(segment->name) != 0) {
    error.sys_errno = errno;
    return IpcMmapUnlinkResult_error_body(
        IPC_ERR_SYSTEM, "system error: shm_unlink failed", error);
  }

  IpcMmapUnmapResult unmap_result = ipc_unmap(segment);
  if (IpcMmapUnmapResult_is_error(unmap_result)) {
    error.sys_errno = unmap_result.error.body.sys_errno;
    error.name = segment->name;
    return IpcMmapUnlinkResult_error_body(unmap_result.ipc_status,
                                          unmap_result.error.detail
                                              ? unmap_result.error.detail
                                              : "illegal state: unmap failed",
                                          error);
  }

  return IpcMmapUnlinkResult_ok(IPC_OK);
}
