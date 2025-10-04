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
  if (name == NULL) {
    IpcMmapCreateError body = {.name = NULL,
                               .requested_size = size,
                               .aligned_size = 0,
                               .page_size = 0,
                               .existing_size = 0,
                               .existed = false,
                               .sys_errno = 0};
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: name is NULL", body);
  }
  if (size == 0) {
    IpcMmapCreateError body = {.name = name,
                               .requested_size = 0,
                               .aligned_size = 0,
                               .page_size = 0,
                               .existing_size = 0,
                               .existed = false,
                               .sys_errno = 0};
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_INVALID_ARGUMENT, "invalid argument: size == 0", body);
  }

  errno = 0;
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    IpcMmapCreateError body = {.name = name,
                               .requested_size = size,
                               .aligned_size = 0,
                               .page_size = -1,
                               .existing_size = 0,
                               .existed = false,
                               .sys_errno = errno};
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: sysconf(_SC_PAGESIZE) failed", body);
  }

  const uint64_t aligned_size = ALIGN_UP(size, (uint64_t)page_size);
  // Попробуем создать новый shm
  errno = 0;
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, OPEN_MODE);
  bool existed = false;
  if (fd >= 0) {
    // Новый — задаём размер
    if (ftruncate(fd, (off_t)aligned_size) < 0) {
      int se = errno;
      close(fd);
      IpcMmapCreateError body = {.name = name,
                                 .requested_size = size,
                                 .aligned_size = aligned_size,
                                 .page_size = page_size,
                                 .existing_size = 0,
                                 .existed = false,
                                 .sys_errno = se};
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: ftruncate failed", body);
    }
  } else {
    if (errno != EEXIST) {
      int se = errno;
      IpcMmapCreateError body = {.name = name,
                                 .requested_size = size,
                                 .aligned_size = aligned_size,
                                 .page_size = page_size,
                                 .existing_size = 0,
                                 .existed = false,
                                 .sys_errno = se};
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: shm_open (create) failed", body);
    }

    // Уже существует — открываем и проверяем размер
    existed = true;
    errno = 0;
    fd = shm_open(name, O_RDWR, OPEN_MODE);
    if (fd < 0) {
      int se = errno;
      IpcMmapCreateError body = {.name = name,
                                 .requested_size = size,
                                 .aligned_size = aligned_size,
                                 .page_size = page_size,
                                 .existing_size = 0,
                                 .existed = true,
                                 .sys_errno = se};
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: shm_open (open) failed", body);
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
      int se = errno;
      close(fd);
      IpcMmapCreateError body = {.name = name,
                                 .requested_size = size,
                                 .aligned_size = aligned_size,
                                 .page_size = page_size,
                                 .existing_size = 0,
                                 .existed = true,
                                 .sys_errno = se};
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_SYSTEM, "system error: fstat failed", body);
    }

    if ((uint64_t)st.st_size != aligned_size) {
      const uint64_t existing_size = (uint64_t)st.st_size;
      close(fd);
      IpcMmapCreateError body = {.name = name,
                                 .requested_size = size,
                                 .aligned_size = aligned_size,
                                 .page_size = page_size,
                                 .existing_size = existing_size,
                                 .existed = true,
                                 .sys_errno = 0};
      return IpcMemorySegmentResult_error_body(
          IPC_ERR_ILLEGAL_STATE,
          "illegal state: existing segment size != aligned size", body);
    }
  }

  // mmap
  errno = 0;
  void *mapped = mmap(NULL, (size_t)aligned_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
  int mmap_errno = errno;
  close(fd);
  if (mapped == MAP_FAILED) {
    IpcMmapCreateError body = {.name = name,
                               .requested_size = size,
                               .aligned_size = aligned_size,
                               .page_size = page_size,
                               .existing_size = 0,
                               .existed = existed,
                               .sys_errno = mmap_errno};
    return IpcMemorySegmentResult_error_body(IPC_ERR_SYSTEM,
                                             "system error: mmap failed", body);
  }

  // Аллоцируем дескриптор
  IpcMemorySegment *segment = (IpcMemorySegment *)malloc(sizeof(*segment));
  if (segment == NULL) {
    munmap(mapped, (size_t)aligned_size);
    IpcMmapCreateError body = {.name = name,
                               .requested_size = size,
                               .aligned_size = aligned_size,
                               .page_size = page_size,
                               .existing_size = 0,
                               .existed = existed,
                               .sys_errno = 0};
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: memory allocation failed", body);
  }

  // Копируем имя
  const size_t nlen = strlen(name) + 1;
  char *name_copy = (char *)malloc(nlen);
  if (name_copy == NULL) {
    munmap(mapped, (size_t)aligned_size);
    free(segment);
    IpcMmapCreateError body = {.name = name,
                               .requested_size = size,
                               .aligned_size = aligned_size,
                               .page_size = page_size,
                               .existing_size = 0,
                               .existed = existed,
                               .sys_errno = 0};
    return IpcMemorySegmentResult_error_body(
        IPC_ERR_SYSTEM, "system error: memory allocation failed", body);
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

  errno = 0;
  if (shm_unlink(segment->name) != 0) {
    IpcMmapUnlinkError body = {.name = segment->name, .sys_errno = errno};
    return IpcMmapUnlinkResult_error_body(
        IPC_ERR_SYSTEM, "system error: shm_unlink failed", body);
  }

  // После успешного unlink — размапим и освободим
  IpcMmapUnmapResult ur = ipc_unmap(segment);
  if (IpcMmapUnmapResult_is_error(ur)) {
    return IpcMmapUnlinkResult_error(
        ur.ipc_status,
        ur.error.detail ? ur.error.detail : "illegal state: unmap failed");
  }

  return IpcMmapUnlinkResult_ok(IPC_OK);
}
