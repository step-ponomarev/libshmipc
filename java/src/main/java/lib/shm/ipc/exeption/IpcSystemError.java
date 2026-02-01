package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcSystemError extends IpcException {
    public IpcSystemError(Throwable cause) {
        super(IpcStatus.IPC_ERR_SYSTEM, cause);
    }

    public IpcSystemError(String message) {
        super(IpcStatus.IPC_ERR_SYSTEM, message);
    }

    public IpcSystemError(String message, Throwable cause) {
        super(IpcStatus.IPC_ERR_SYSTEM, message, cause);
    }
}
