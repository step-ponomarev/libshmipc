package ipc.exception;

import ipc.error.IpcErrorCode;

// ipc_error_internal_corrupted
public final class IpcInternalCorruptedException extends IpcInternalException {
    public final long offset;

    public IpcInternalCorruptedException(String message, long offset) {
        super(IpcErrorCode.IPC_ERR_CODE_ENTRY_CORRUPTED, message);
        this.offset = offset;
    }
}
