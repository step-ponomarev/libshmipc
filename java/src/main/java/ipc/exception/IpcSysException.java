package ipc.exception;

import ipc.error.IpcErrorCode;
import ipc.error.IpcErrorKind;

// ipc_error_sys
public final class IpcSysException extends IpcException {
    public final int sysErrno;

    public IpcSysException(IpcErrorCode code, String message, int sysErrno) {
        super(IpcErrorKind.IPC_ERR_KIND_SYS, code, message);
        this.sysErrno = sysErrno;
    }
}
