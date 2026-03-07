package ipc.error;

import jextract.ipc_error_h;

public enum IpcErrorKind {
    IPC_ERR_KIND_NONE(ipc_error_h.IPC_ERR_KIND_NONE()),
    IPC_ERR_KIND_ARG(ipc_error_h.IPC_ERR_KIND_ARG()),
    IPC_ERR_KIND_INTERNAL(ipc_error_h.IPC_ERR_KIND_INTERNAL()),
    IPC_ERR_KIND_SYS(ipc_error_h.IPC_ERR_KIND_SYS());

    public final int value;

    IpcErrorKind(int value) {
        this.value = value;
    }

    public static IpcErrorKind of(int value) {
        for (var kind : values()) {
            if (kind.value == value) {
                return kind;
            }
        }
        throw new IllegalArgumentException("Unknown ipc_error_kind_t: " + value);
    }
}
