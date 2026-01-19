package lib.shm.ipc.exeption;

import lib.shm.ipc.IpcStatus;

public final class IpcException extends Exception {

    public IpcException(IpcStatus status, Throwable cause) {
        super(cause);
        this.status = status;
    }

    private final IpcStatus status;

    public IpcException(IpcStatus status, String message) {
        super(message);
        this.status = status;
    }

    public IpcException(IpcStatus status, String message, Throwable cause) {
        super(message, cause);
        this.status = status;
    }

    public IpcStatus getStatus() {
        return status;
    }
}
