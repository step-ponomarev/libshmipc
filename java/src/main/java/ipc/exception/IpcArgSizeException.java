package ipc.exception;

import ipc.error.IpcErrorCode;

// ipc_error_arg_size
public final class IpcArgSizeException extends IpcArgException {
    public final long limit;
    public final long requestedSize;
    public final long suggestedSize;

    public IpcArgSizeException(IpcErrorCode code, String message, long limit, long requestedSize, long suggestedSize) {
        super(code, message);
        this.limit = limit;
        this.requestedSize = requestedSize;
        this.suggestedSize = suggestedSize;
    }
}
