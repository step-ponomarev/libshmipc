package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcUnknownException extends IpcException {
    public IpcUnknownException(Throwable cause) {
        super(IpcStatus.IPC_UNKNOWN, cause);
    }

    public IpcUnknownException(String message) {
        super(IpcStatus.IPC_UNKNOWN, message);
    }

    public IpcUnknownException(String message, Throwable cause) {
        super(IpcStatus.IPC_UNKNOWN, message, cause);
    }
}
