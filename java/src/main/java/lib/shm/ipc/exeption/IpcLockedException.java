package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcLockedException extends IpcException {
    public IpcLockedException(Throwable cause) {
        super(IpcStatus.IPC_ERR_LOCKED, cause);
    }

    public IpcLockedException(String message) {
        super(IpcStatus.IPC_ERR_LOCKED, message);
    }

    public IpcLockedException(String message, Throwable cause) {
        super(IpcStatus.IPC_ERR_LOCKED, message, cause);
    }
}
