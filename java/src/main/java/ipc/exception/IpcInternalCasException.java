package ipc.exception;

import ipc.error.IpcCasTarget;
import ipc.error.IpcErrorCode;

// ipc_error_internal_cas
public final class IpcInternalCasException extends IpcInternalException {
    public final IpcCasTarget target;
    public final long expectedOffset;
    public final long actualOffset;
    public final long desiredOffset;

    public IpcInternalCasException(IpcErrorCode code, String message, IpcCasTarget target, long expectedOffset, long actualOffset, long desiredOffset) {
        super(code, message);
        this.target = target;
        this.expectedOffset = expectedOffset;
        this.actualOffset = actualOffset;
        this.desiredOffset = desiredOffset;
    }
}
