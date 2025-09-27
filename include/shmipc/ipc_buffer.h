#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>

SHMIPC_BEGIN_DECLS

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
SHMIPC_API uint64_t ipc_buffer_allign_size(uint64_t size);

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
SHMIPC_API IpcBuffer *ipc_buffer_create(void *mem, const uint64_t size);

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
SHMIPC_API IpcBuffer *ipc_buffer_attach(void *mem);

/**
 * @brief Writes a payload to the IPC buffer.
 *
 * Attempts to write the given data as a single entry into the IPC buffer.
 * This function ensures the payload is written and made available to readers
 * as a complete, committed entry.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @param data Pointer to the data to be written.
 * @param size Size (in bytes) of the data to write.
 * @return `IPC_OK` on success, or an appropriate error code:
 *         - `IPC_ERR_INVALID_ARGUMENT` if any input is invalid.
 *         - `IPC_ERR_ENTRY_TOO_LARGE` if the data is too large for the buffer.
 *         - `IPC_NO_SPACE_CONTIGUOUS` if there is insufficient space to write
 * the entry.
 *
 * @note The write is treated as an atomic operation: data is either fully
 * written and published, or not written at all.
 */
SHMIPC_API IpcStatus ipc_buffer_write(IpcBuffer *buffer, const void *data,
                                      const uint64_t size);

/**
 * @brief Reads the next available entry from the IPC buffer.
 *
 * Attempts to read the next committed entry from the buffer into the
 * provided destination structure. On success, the payload is copied into
 * the memory pointed to by `dest->payload`, and the actual payload size is
 * set in `dest->size`. The entry is marked as consumed by advancing the
 * buffer's read pointer.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @param dest Pointer to an `IpcEntry` structure with a pre-allocated payload
 * buffer. The `size` field must indicate the maximum capacity of the payload
 * buffer.
 * @return An `IpcTransaction` indicating the result of the operation:
 *         - `IPC_OK` on success.
 *         - `IPC_ERR_INVALID_ARGUMENT` if inputs are invalid.
 *         - `IPC_ERR_TOO_SMALL` if the destination buffer is too small for the
 * entry.
 *         - `IPC_NOT_READY` or `IPC_LOCKED` if the next entry is temporarily
 * not accessible.
 *         - `IPC_CORRUPTED` if the entry is malformed or inconsistent.
 *
 * @note If the entry is marked as `NOT_READY` or `LOCKED`, it is temporarily
 * unreadable. The caller may retry the operation later; the buffer state has
 * not been modified.
 *
 * @warning The caller must ensure that `dest->payload` points to valid memory
 * capable of holding at least `dest->size` bytes.
 */
SHMIPC_API IpcTransaction ipc_buffer_read(IpcBuffer *buffer, IpcEntry *dest);

/**
 * @brief Inspects the next available entry in the IPC buffer without consuming
 * it.
 *
 * Retrieves a pointer to the payload of the next committed entry in the buffer,
 * allowing read-only access without advancing the buffer's read pointer.
 * Useful for previewing data or implementing zero-copy access.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @param dest Pointer to an `IpcEntry` structure that will be populated with
 * the payload pointer and size.
 * @return An `IpcTransaction` indicating the result of the operation:
 *         - `IPC_OK` on success.
 *         - `IPC_ERR_INVALID_ARGUMENT` if inputs are invalid.
 *         - `IPC_NOT_READY` or `IPC_LOCKED` if the next entry is temporarily
 * not accessible.
 *         - `IPC_CORRUPTED` if the entry is malformed or inconsistent.
 *
 * @note If the entry is marked as `NOT_READY` or `LOCKED`, this indicates that
 * the entry exists but is temporarily unreadable. The caller may retry the
 * operation later. The buffer state is not changed by this call.
 *
 * @warning The returned payload pointer is only valid while the entry remains
 * in place and unmodified in the buffer. The caller must not alter or free the
 * memory.
 */
SHMIPC_API IpcTransaction ipc_buffer_peek(const IpcBuffer *buffer,
                                          IpcEntry *dest);

/**
 * @brief Skips the next entry in the IPC buffer without reading its contents.
 *
 * Advances the buffer's read pointer past the next entry, discarding it without
 * copying the payload. This is useful when a consumer chooses to ignore or
 * reject an entry based on its metadata or context.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @param id Entry ID to skip, which must match the current read position.
 * @return An `IpcTransaction` indicating the result of the operation:
 *         - `IPC_OK` on success.
 *         - `IPC_ERR_INVALID_ARGUMENT` if the input is invalid.
 *         - `IPC_EMPTY` if the buffer contains no unread entries.
 *         - `IPC_LOCKED` if the entry is currently locked and cannot be
 * skipped.
 *         - `IPC_ALREADY_SKIPED` if another thread already skipped this entry.
 *
 * @note The caller must pass the exact entry ID previously obtained
 * (e.g., from `ipc_buffer_peek` or `ipc_buffer_read`) to ensure safe and
 * consistent behavior.
 *
 * @warning This operation may race with a producer finalizing the entry (e.g.,
 * changing it from `NOT_READY` to `READY`). In such cases, skipping ensures the
 * entry is locked and not consumed.
 */
SHMIPC_API IpcTransaction ipc_buffer_skip(IpcBuffer *buffer,
                                          const IpcEntryId id);

/**
 * @brief Forcefully skips the next entry in the IPC buffer.
 *
 * Unconditionally advances the buffer's read pointer past the next entry,
 * regardless of its state. This function is intended for recovery or
 * low-level maintenance scenarios where the caller explicitly wants to discard
 * the current entry without regard to its readiness or integrity.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @return An `IpcTransaction` indicating the result of the operation:
 *         - `IPC_OK` on success.
 *         - `IPC_ERR_INVALID_ARGUMENT` if the input is invalid.
 *         - `IPC_EMPTY` if the buffer contains no unread entries.
 *         - `IPC_ALREADY_SKIPED` if another thread already advanced the read
 * pointer.
 *
 * @note This function does not validate the contents or state of the entry.
 * It assumes the caller understands the implications of discarding the entry
 * blindly.
 *
 * @warning This operation may skip over incomplete, locked, or corrupted
 * entries. Use with caution in production systems. Prefer `ipc_buffer_skip`
 * when possible.
 */
SHMIPC_API IpcTransaction ipc_buffer_skip_force(IpcBuffer *buffer);

/**
 * @brief Reserves space for a new entry in the IPC buffer.
 *
 * Attempts to reserve a contiguous block of memory within the IPC buffer for
 * writing a new entry of the specified size. This function is typically used by
 * the producer side to prepare a payload to be written to the shared buffer.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @param size The size (in bytes) of the payload to reserve.
 * @param dest Output pointer that will be set to the start of the writable
 * memory region within the buffer, or `NULL` on failure.
 * @return An `IpcTransaction` indicating the result of the operation.
 *         - `IPC_OK` on success.
 *         - `IPC_ERR_INVALID_ARGUMENT` if any input is invalid.
 *         - `IPC_ERR_ENTRY_TOO_LARGE` if the requested entry is too large to
 * fit in the buffer.
 *         - `IPC_NO_SPACE_CONTIGUOUS` if there is not enough contiguous free
 * space.
 *
 * @note This function does not write the payload itself. The caller must fill
 * the reserved region pointed to by `*dest` and finalize the entry separately
 * by calling `ipc_buffer_commit_entry`.
 * If the buffer wraps around during the reservation, a wrap marker is inserted.
 *
 * @warning The reserved entry must be committed using `ipc_buffer_commit_entry`
 * to make it visible to consumers. Failing to commit may result in lost space
 * or inconsistent buffer state.
 */
SHMIPC_API IpcTransaction ipc_buffer_reserve_entry(IpcBuffer *buffer,
                                                   const uint64_t size,
                                                   void **dest);

/**
 * @brief Marks a previously reserved entry as ready for consumption.
 *
 * Finalizes an entry in the IPC buffer by updating its metadata to indicate
 * that the payload is fully written and can be read by consumers.
 *
 * @param buffer Pointer to a valid `IpcBuffer` structure.
 * @param id The entry ID returned by `ipc_buffer_reserve_entry`.
 * @return `IPC_OK` on success, or an appropriate error code:
 *         - `IPC_ERR_INVALID_ARGUMENT` if the buffer pointer is NULL.
 *         - `IPC_ERR` if the entry is in an invalid or unexpected state.
 *
 * @note This function should be called only after a successful call to
 * `ipc_buffer_reserve_entry` and after the payload has been written.
 * It ensures that the entry becomes visible to the reader.
 *
 * @warning Calling this function on an entry that is already committed or not
 * properly reserved results in undefined behavior and possible data corruption.
 */

SHMIPC_API IpcStatus ipc_buffer_commit_entry(IpcBuffer *buffer,
                                             const IpcEntryId id);

SHMIPC_END_DECLS
