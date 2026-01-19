package lib.shm.ipc;

import jextract.ipc_common_h;

public enum IpcStatus {
    //TODO: не все из них нужны тут?
    IPC_OK(ipc_common_h.IPC_OK()),
    IPC_EMPTY(ipc_common_h.IPC_EMPTY()),
    IPC_ALREADY_SKIPPED(ipc_common_h.IPC_ALREADY_SKIPPED()),
    IPC_PLACEHOLDER(ipc_common_h.IPC_PLACEHOLDER()),
    IPC_ERR_ENTRY_TOO_LARGE(ipc_common_h.IPC_ERR_ENTRY_TOO_LARGE()),
    IPC_ERR_ALLOCATION(ipc_common_h.IPC_ERR_ALLOCATION()),
    IPC_ERR_INVALID_ARGUMENT(ipc_common_h.IPC_ERR_INVALID_ARGUMENT()),
    IPC_ERR_TOO_SMALL(ipc_common_h.IPC_ERR_TOO_SMALL()),
    IPC_ERR_ILLEGAL_STATE(ipc_common_h.IPC_ERR_ILLEGAL_STATE()),
    IPC_ERR_SYSTEM(ipc_common_h.IPC_ERR_SYSTEM()),
    IPC_ERR_NO_SPACE_CONTIGUOUS(ipc_common_h.IPC_ERR_NO_SPACE_CONTIGUOUS()),
    IPC_ERR_NOT_READY(ipc_common_h.IPC_ERR_NOT_READY()),
    IPC_ERR_LOCKED(ipc_common_h.IPC_ERR_LOCKED()),
    IPC_ERR_OFFSET_MISMATCH(ipc_common_h.IPC_ERR_OFFSET_MISMATCH()),
    IPC_ERR_TIMEOUT(ipc_common_h.IPC_ERR_TIMEOUT()),
    IPC_ERR_CORRUPTED(ipc_common_h.IPC_ERR_CORRUPTED()),
    IPC_UNKNOWN(404);
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

        return IpcStatus.IPC_UNKNOWN;
    }

    public int getStatus() {
        return status;
    }
}