package ipc.error;

import jextract.ipc_error_h;

public enum IpcCasTarget {
    NONE(ipc_error_h.IPC_CAS_TARGET_NONE()),
    TAIL(ipc_error_h.IPC_CAS_TARGET_TAIL()),
    HEAD(ipc_error_h.IPC_CAS_TARGET_HEAD());

    public final int value;

    IpcCasTarget(int v) {
        this.value = v;
    }

    public static IpcCasTarget of(int v) {
        for (var t : values())
            if (t.value == v) {
                return t;
            }

        throw new IllegalArgumentException("Unknown ipc_cas_target_t: " + v);
    }
}
