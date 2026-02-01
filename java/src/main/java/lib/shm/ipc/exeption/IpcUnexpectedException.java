package lib.shm.ipc.exeption;

public final class IpcUnexpectedException extends RuntimeException {
    public IpcUnexpectedException(String message) {
        super(message);
    }

    public IpcUnexpectedException(Throwable cause) {
        super(cause);
    }
}
