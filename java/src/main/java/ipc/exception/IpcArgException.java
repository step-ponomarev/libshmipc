package ipc.exception;

import ipc.error.IpcErrorCode;
import ipc.error.IpcErrorKind;

// ipc_error_arg
public class IpcArgException extends IpcException {
    public IpcArgException(IpcErrorCode code, String message) {
        super(IpcErrorKind.IPC_ERR_KIND_ARG, code, message);
    }
}
