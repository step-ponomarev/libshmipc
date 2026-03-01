package ipc.exception;

public final class IpcUnknownException extends RuntimeException {
    public IpcUnknownException(String message, Throwable cause) {
        super(message, cause);
    }

    public IpcUnknownException(Throwable cause) {
        super(cause);
    }
}
