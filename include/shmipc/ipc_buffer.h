#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_common.h"
#include <stdint.h>

typedef struct IpcBuffer IpcBuffer;

/**
 * @brief Returns the total memory size required to create an IPC buffer.
 *
 * This function adds the space needed for internal metadata to the user-defined
 * data size. It is useful when allocating a memory region (e.g., with `malloc`,
 * `mmap`, or shared memory APIs) intended for use with `ipc_buffer_create`.
 *
 * @param size The desired size of the usable data area, in bytes.
 * @return The total size in bytes, including internal metadata.
 */
uint64_t ipc_buffer_allign_size(uint64_t);

/**
 * @brief Initializes an IPC buffer within a pre-allocated memory region.
 *
 * Uses the provided memory pointer to store internal metadata and buffer data.
 * Typically called after allocating memory with `malloc`, `mmap`, `shmget`, or
 * similar mechanisms.
 *
 * @param mem Pointer to a pre-allocated memory block.
 * @param size Total size of the memory block in bytes, including metadata.
 * @return A pointer to an initialized `IpcBuffer`, or `NULL` on failure (e.g.,
 * insufficient memory or internal allocation failure).
 *
 * @note To calculate the correct memory size, it is recommended to use
 * `ipc_buffer_allign_size()`.
 */
IpcBuffer *ipc_buffer_create(void *, const uint64_t);

/**
 * @brief Attaches to an existing IPC buffer in shared memory.
 *
 * Initializes an `IpcBuffer` structure using a pointer to a memory region
 * previously initialized by `ipc_buffer_create`. This is typically used on the
 * consumer or client side to connect to a shared buffer created elsewhere.
 *
 * @param mem Pointer to a memory region containing a valid IPC buffer.
 * @return A pointer to an `IpcBuffer` instance, or `NULL` on failure (e.g., if
 * memory allocation fails or the input is `NULL`).
 *
 * @note This function does not validate the contents of the memory block.
 * It assumes the memory has been properly initialized by `ipc_buffer_create`.
 */
IpcBuffer *ipc_buffer_attach(void *);
IpcStatus ipc_buffer_write(IpcBuffer *, const void *, const uint64_t);
IpcTransaction ipc_buffer_read(IpcBuffer *, IpcEntry *);
IpcTransaction ipc_buffer_peek(IpcBuffer *, IpcEntry *);
IpcTransaction ipc_buffer_skip(IpcBuffer *, const IpcEntryId);
IpcTransaction ipc_buffer_skip_force(IpcBuffer *);
IpcTransaction ipc_buffer_reserve_entry(IpcBuffer *, const uint64_t, void **);
IpcStatus ipc_buffer_commit_entry(IpcBuffer *, const IpcEntryId);

#ifdef __cplusplus
}
#endif

#endif
