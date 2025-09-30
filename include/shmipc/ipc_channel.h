#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

SHMIPC_BEGIN_DECLS

typedef struct IpcChannel IpcChannel;

typedef struct IpcChannelResult IpcChannelResult;
IPC_RESULT(IpcChannelResult, IpcChannel *)

typedef struct IpcChannelConfiguration {
  uint32_t max_round_trips;
  long start_sleep_ns;
  long max_sleep_ns;
} IpcChannelConfiguration;

/**
 * @brief Returns the minimum aligned size required for an IPC channel buffer.
 *
 * The returned size is the smallest multiple of internal alignment constraints
 * that can safely be used to back a shared memory buffer for the channel.
 * It ensures the channel's internal structures are properly aligned.
 *
 * @param size Requested data buffer size in bytes.
 * @return Aligned size in bytes that satisfies internal constraints.
 *
 * @note Use this function to allocate memory for `ipc_channel_create`.
 */
SHMIPC_API uint64_t ipc_channel_allign_size(uint64_t size);

/**
 * @brief Initializes a new IPC channel on the given memory region.
 *
 * This function creates and configures an `IpcChannel` instance using
 * the provided memory block. The memory must be large enough and properly
 * aligned as determined by `ipc_channel_allign_size`. The memory is treated
 * as shared and must be accessible by both communicating parties.
 *
 * @param mem Pointer to a memory block to back the channel's buffer.
 * @param size Size of the memory block in bytes.
 * @param config Configuration for retry behavior and wait timings.
 * @return Pointer to the newly created `IpcChannel`, or `NULL` on failure.
 *
 * @note This function allocates memory internally for the channel metadata.
 *       It does not take ownership of the memory pointed to by `mem`.
 *       Use `ipc_channel_destroy()` to deallocate the channel structure.
 */
SHMIPC_API IpcChannelResult ipc_channel_create(
    void *mem, const uint64_t size, const IpcChannelConfiguration config);

/**
 * @brief Connects to an existing IPC channel using a shared memory region.
 *
 * This function is used by clients or readers to attach to a pre-initialized
 * IPC channel that has been created with `ipc_channel_create`. It assumes that
 * the shared memory region pointed to by `mem` is already initialized and
 * aligned using `ipc_channel_allign_size`.
 *
 * @param mem Pointer to a shared memory region that backs the IPC channel.
 * @param config Configuration parameters for retry and delay behavior.
 * @return Pointer to a new IpcChannel handle on success, or NULL on failure.
 *
 * @note The `mem` must point to a valid memory region previously passed to
 * `ipc_channel_create`.
 * @note The returned `IpcChannel *` must be released using
 * `ipc_channel_destroy` to free associated resources.
 */
SHMIPC_API IpcChannelResult
ipc_channel_connect(void *mem, const IpcChannelConfiguration config);

/**
 * @brief Destroys an IPC channel and releases associated resources.
 *
 * This function should be called when an `IpcChannel` is no longer needed.
 * It frees the internal buffer reference and the channel structure itself.
 * Note that this does not unmap or deallocate the shared memory region
 * provided to `ipc_channel_create` or `ipc_channel_connect`; it only releases
 * the local heap allocations made during channel setup.
 *
 * @param channel Pointer to the IpcChannel to destroy. Can be safely NULL.
 * @return IPC_OK on successful destruction, or IPC_ERR_INVALID_ARGUMENT if
 *         the `channel` pointer is NULL.
 *
 * @note It is the caller’s responsibility to ensure no other threads are
 *       using the channel when this function is called.
 */
SHMIPC_API IpcStatusResult ipc_channel_destroy(IpcChannel *channel);

/**
 * @brief Writes a payload into the IPC channel.
 *
 * This function attempts to write a complete entry to the channel's underlying
 * buffer. The write is atomic—data is either fully written and published,
 * or not written at all.
 *
 * @param channel Pointer to an initialized `IpcChannel` instance.
 * @param data Pointer to the data to write.
 * @param size Size of the data in bytes.
 * @return `IPC_OK` on success, or an appropriate error code:
 *         - `IPC_ERR_INVALID_ARGUMENT` if any parameter is invalid.
 *         - `IPC_ERR_ENTRY_TOO_LARGE` if the data is too large to fit in the
 * buffer.
 *         - `IPC_NO_SPACE_CONTIGUOUS` if the buffer lacks sufficient contiguous
 * space.
 *         - `IPC_ERR` if the channel is not properly connected or initialized.
 *
 * @note This function does not block; it returns immediately even if the buffer
 *       is full or unprepared.
 */
SHMIPC_API IpcStatusResult ipc_channel_write(IpcChannel *channel,
                                             const void *data,
                                             const uint64_t size);

/**
 * @brief Blocking read from the IPC channel.
 *
 * Attempts to read the next available entry from the channel. If no data is
 * immediately available, the function retries using peek-based probing,
 * bounded by a maximum number of retries configured in the channel.
 *
 * Memory is allocated internally for the payload, and ownership of this memory
 * is transferred to the caller via the `dest->payload` field. The caller must
 * free this memory when done.
 *
 * @param channel Pointer to an initialized `IpcChannel`.
 * @param dest Pointer to an `IpcEntry` that will receive the payload and size.
 *             The `.payload` field will be dynamically allocated and must be
 * freed.
 * @return `IpcTransaction` with:
 *         - `status == IPC_OK` on success.
 *         - `IPC_EMPTY`, `IPC_NOT_READY`, `IPC_LOCKED`, `IPC_CORRUPTED` for
 * retryable conditions.
 *         - `IPC_REACHED_RETRY_LIMIT` if maximum retries were exceeded.
 *         - `IPC_ERR_ALLOCATION` if memory allocation failed.
 *         - `IPC_ERR_INVALID_ARGUMENT` or `IPC_ERR` for general errors.
 *
 * @note If called via `ipc_channel_read_with_timeout`, it will timeout after
 * the specified duration, returning `IPC_TIMEOUT`.
 */
SHMIPC_API IpcTransactionResult ipc_channel_read(IpcChannel *channel,
                                                 IpcEntry *dest);

/**
 * @brief Reads the next available entry from the IPC channel with a timeout.
 *
 * This function attempts to read an entry from the channel within the given
 * timeout period. If no valid entry becomes available before the timeout, the
 * operation fails with `IPC_TIMEOUT`.
 *
 * @param channel Pointer to a valid `IpcChannel` instance.
 * @param dest Pointer to a user-provided `IpcEntry` structure that will receive
 *             the payload data and size if the operation is successful.
 *             The function allocates memory for `dest->payload` which must be
 *             freed by the caller.
 * @param timeout Maximum duration to wait for a readable entry. If the timeout
 *                expires before a valid entry becomes available,
 *                `IPC_TIMEOUT` is returned.
 *
 * @return A `IpcTransaction` containing:
 *         - `status == IPC_OK` if the read was successful.
 *         - `status == IPC_TIMEOUT` if the timeout was reached.
 *         - `status == IPC_ERR_INVALID_ARGUMENT` if arguments are invalid.
 *         - `status == IPC_ERR` for internal errors (e.g., `clock_gettime`
 * failure).
 *         - `status == IPC_CORRUPTED`, `IPC_LOCKED`, `IPC_NOT_READY` if
 *           the entry at the head was not readable.
 *
 * @note If `IPC_TIMEOUT` is returned, the `entry_id` field will contain the
 *       identifier of the last observed entry. You may inspect the state of
 * this entry using `ipc_channel_peek()` to diagnose the cause of the stall.
 */
SHMIPC_API IpcTransactionResult ipc_channel_read_with_timeout(
    IpcChannel *channel, IpcEntry *dest, const struct timespec *timeout);

/**
 * @brief Attempts a non-blocking read from the IPC channel.
 *
 * This function checks for the availability of a committed entry in the channel
 * and reads it if present. Unlike `ipc_channel_read`, this call does not block
 * and returns immediately if no entry is available.
 *
 * On success, memory for the payload is allocated and assigned to
 * `dest->payload`. The caller is responsible for freeing it using `free`.
 *
 * @param channel Pointer to a valid `IpcChannel`.
 * @param dest Pointer to an `IpcEntry` that will be populated on success.
 * @return IpcTransaction with status and associated entry ID:
 *         - `IPC_OK`: A valid entry was read successfully.
 *         - `IPC_EMPTY`: No entries are currently available.
 *         - `IPC_NOT_READY`, `IPC_LOCKED`, `IPC_CORRUPTED`: Entry is not
 * readable yet or buffer is in an invalid state.
 *         - `IPC_ERR_ALLOCATION`: Memory allocation failed during the read
 * attempt.
 *         - `IPC_ERR_INVALID_ARGUMENT`: One or more arguments are invalid.
 *         - `IPC_ERR`: Unspecified internal error.
 *
 * @note The function guarantees that `dest` is not modified unless the read
 * succeeds.
 */
SHMIPC_API IpcTransactionResult ipc_channel_try_read(IpcChannel *channel,
                                                     IpcEntry *dest);

/**
 * @brief Peeks at the next available entry in the IPC channel without consuming
 * it.
 *
 * This function allows inspecting the next committed entry in the channel
 * without removing it from the buffer. Useful for previewing data or performing
 * conditional logic before actual consumption.
 *
 * @param channel Pointer to a valid `IpcChannel` instance.
 * @param dest Pointer to an `IpcEntry` structure that will be populated with
 * entry metadata. The payload pointer will reference internal buffer memory and
 * should not be freed.
 * @return IpcTransaction containing the result:
 *         - `IPC_OK`: A committed entry is available for reading.
 *         - `IPC_EMPTY`: The buffer is currently empty.
 *         - `IPC_NOT_READY`, `IPC_LOCKED`, `IPC_CORRUPTED`: Buffer is
 * temporarily unreadable or in error state.
 *         - `IPC_ERR_INVALID_ARGUMENT`: Null input detected.
 *         - `IPC_ERR`: Unspecified internal failure.
 *
 * @note The payload returned remains owned by the channel. Do not modify or
 * free it. For persistent access, copy the contents into a user-managed buffer.
 */
SHMIPC_API IpcTransactionResult ipc_channel_peek(const IpcChannel *channel,
                                                 IpcEntry *dest);

/**
 * @brief Skips the specified entry in the IPC channel by its ID.
 *
 * This function marks a specific entry as skipped, allowing the reader to
 * advance past corrupted or unwanted data without consuming it. The entry must
 * match the one currently visible to the reader.
 *
 * @param channel Pointer to a valid `IpcChannel` instance.
 * @param id Entry identifier returned from a previous peek operation.
 * @return IpcTransaction containing the result:
 *         - `IPC_OK`: Entry was successfully skipped.
 * current entry.
 *         - `IPC_ALREADY_SKIPED`: The entry has already been skipped by another
 * reader.
 *         - `IPC_ERR_INVALID_ARGUMENT`: Null channel or invalid input.
 *         - `IPC_ERR`: Internal error or buffer misconfiguration.
 *
 * @note Skipping is useful for handling corrupted or unprocessable entries in a
 * robust IPC setup.
 */
IpcStatusResult ipc_channel_skip(IpcChannel *channel, const IpcEntryId id);

/**
 * @brief Forcibly skips the current entry in the IPC channel.
 *
 * This function unconditionally advances the reader position, skipping the
 * current entry regardless of its state (e.g., corrupted or uncommitted). It is
 * typically used as a recovery mechanism when the reader is unable to make
 * progress using regular skip logic.
 *
 * @param channel Pointer to a valid `IpcChannel` instance.
 * @return IpcTransaction containing the result:
 *         - `IPC_OK`: Entry was successfully skipped.
 *         - `IPC_EMPTY`: No entries were present to skip.
 *         - `IPC_ALREADY_SKIPED`: Entry was already skipped.
 *         - `IPC_ERR_INVALID_ARGUMENT`: Null channel input.
 *         - `IPC_ERR`: Internal error or invalid buffer state.
 *
 * @note It is generally recommended to use `ipc_channel_skip` instead,
 *       as it performs consistency checks before skipping. The force
 *       variant should be reserved for error recovery scenarios.
 */
SHMIPC_API IpcTransactionResult ipc_channel_skip_force(IpcChannel *channel);

SHMIPC_END_DECLS
