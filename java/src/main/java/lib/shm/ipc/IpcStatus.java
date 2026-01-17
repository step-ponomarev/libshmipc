package lib.shm.ipc;

public enum IpcStatus {
    //TODO: не все из них нужны тут?
    IPC_OK(0),
    IPC_ALREADY_SKIPPED(2),
    IPC_PLACEHOLDER(3),
    IPC_ERR_ENTRY_TOO_LARGE(-1),
    IPC_ERR_ALLOCATION(-1),
    IPC_ERR_INVALID_ARGUMENT(-3),
    IPC_ERR_TOO_SMALL(-4),
    IPC_ERR_ILLEGAL_STATE(-5),
    IPC_ERR_SYSTEM(-6),
    IPC_ERR_NO_SPACE_CONTIGUOUS(-7),
    IPC_ERR_NOT_READY(-8),
    IPC_ERR_LOCKED(-9),
    IPC_ERR_OFFSET_MISMATCH(-10),
    IPC_ERR_TIMEOUT(-11),
    IPC_ERR_CORRUPTED(-12),
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