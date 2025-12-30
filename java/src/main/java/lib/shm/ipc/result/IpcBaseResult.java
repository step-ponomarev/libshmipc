package lib.shm.ipc.result;

public abstract class IpcBaseResult {
    private final IpcError error;

    protected IpcBaseResult(IpcError error) {
        this.error = error;
    }

    public final boolean isError() {
        return getError() != null;
    }

    public final IpcError getError() {
        return error;
    }
}
