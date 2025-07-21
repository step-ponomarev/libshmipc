#ifndef IPC_MMAP_H
#define IPC_MMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shmipc/ipc_common.h>

typedef enum { SYSTEM_ERR = -1, INVALID_SIZE = -2 } IpcMmapError;
typedef struct IpcMemorySegment {
  char *name;
  uint64_t size;
  void *memory;
} IpcMemorySegment;

/**
 * @brief Opens or creates a shared memory segment and maps it into the process
 * address space.
 *
 * Maps a named POSIX shared memory segment into memory with read and write
 * permissions. On success, returns a handle to the mapped memory segment. On
 * failure, sets an error code.
 *
 * @param name   Name of the shared memory segment (must be a null-terminated
 * string).
 * @param size   Size in bytes to allocate or check for the segment.
 * @param err    Output parameter for error reporting. Must not be NULL.
 *
 * @return Pointer to an `IpcMemorySegment` structure on success, or NULL on
 * failure.
 *
 * @note The returned segment must be released using `ipc_unmap` or `ipc_unlink`
 * when no longer needed.
 * @note On error, the value pointed to by `err` is set to the appropriate error
 * code.
 */
IpcMemorySegment *ipc_mmap(const char *name, const uint64_t size,
                           IpcMmapError *err);

/**
 * @brief Unmaps and releases a shared memory segment.
 *
 * Unmaps the memory associated with the given shared memory segment and
 * releases all related resources. After calling this function, the `segment`
 * pointer is invalid and must not be used.
 *
 * @param segment Pointer to an `IpcMemorySegment` previously returned by
 * `ipc_mmap`. May be NULL.
 * @return `IPC_OK` on success; `IPC_ERR_INVALID_ARGUMENT` if `segment` or its
 * memory pointer is NULL; other error codes on failure.
 *
 * @note Does not remove the shared memory object itself; use `ipc_unlink` to
 * unlink the underlying segment.
 */
IpcStatus ipc_unmap(IpcMemorySegment *segment);

/**
 * @brief Unlinks and unmaps a shared memory segment.
 *
 * Removes the named shared memory object from the system and unmaps the
 * associated memory region, releasing all allocated resources.
 *
 * @param segment Pointer to an `IpcMemorySegment` previously returned by
 * `ipc_mmap`.
 * @return `IPC_OK` on success; `IPC_ERR_INVALID_ARGUMENT` if `segment` is NULL;
 * other error codes on failure.
 *
 * @note After calling this function, the `segment` pointer is invalid and must
 * not be used.
 * @note This operation is irreversible; the shared memory object will be
 * deleted once all processes have closed it.
 */
IpcStatus ipc_unlink(IpcMemorySegment *segment);

#ifdef __cplusplus
}
#endif

#endif
