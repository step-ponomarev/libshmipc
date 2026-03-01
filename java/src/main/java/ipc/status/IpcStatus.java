package ipc.status;

import jextract.ipc_status_h;

public enum IpcStatus {
    IPC_STATUS_OK(ipc_status_h.IPC_STATUS_OK()),
    IPC_STATUS_EMPTY(ipc_status_h.IPC_STATUS_EMPTY()),
    IPC_STATUS_BUSY(ipc_status_h.IPC_STATUS_BUSY()),
    IPC_STATUS_TIMEOUT(ipc_status_h.IPC_STATUS_TIMEOUT()),
    IPC_STATUS_NO_SPACE(ipc_status_h.IPC_STATUS_NO_SPACE()),
    IPC_STATUS_ERROR(ipc_status_h.IPC_STATUS_ERROR());
    private final int status;

    IpcStatus(int status) {
        this.status = status;
    }

    public static IpcStatus of(int status) {
        for (IpcStatus i : IpcStatus.values()) {
            if (i.status == status) {
                return i;
            }
        }

        throw new IllegalArgumentException("Unknown ipc_status_t: " + status);
    }

    public int getStatus() {
        return status;
    }
}