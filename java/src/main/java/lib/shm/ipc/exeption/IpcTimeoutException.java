package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcTimeoutException extends IpcException {
    public IpcTimeoutException(Throwable cause) {
        super(IpcStatus.IPC_ERR_TIMEOUT, cause);
    }

    public IpcTimeoutException(String message) {
        super(IpcStatus.IPC_ERR_TIMEOUT, message);
    }

    public IpcTimeoutException(String message, Throwable cause) {
        super(IpcStatus.IPC_ERR_TIMEOUT, message, cause);
    }
}
