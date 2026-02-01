package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcReadException extends IpcException {
    public IpcReadException(IpcStatus status, Throwable cause) {
        super(status, cause);
    }

    public IpcReadException(IpcStatus status, String message) {
        super(status, message);
    }

    public IpcReadException(IpcStatus status, String message, Throwable cause) {
        super(status, message, cause);
    }
}
