package ipc.exception;

import ipc.error.IpcErrorCode;
import ipc.error.IpcErrorKind;

public abstract class IpcInternalException extends IpcException {
    public IpcInternalException(IpcErrorCode code, String message) {
        super(IpcErrorKind.IPC_ERR_KIND_INTERNAL, code, message);
    }
}
