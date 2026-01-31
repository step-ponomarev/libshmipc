package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcWriteException extends IpcException {
    public IpcWriteException(IpcStatus status, Throwable cause) {
        super(status, cause);
    }

    public IpcWriteException(IpcStatus status, String message) {
        super(status, message);
    }

    public IpcWriteException(IpcStatus status, String message, Throwable cause) {
        super(status, message, cause);
    }
}
