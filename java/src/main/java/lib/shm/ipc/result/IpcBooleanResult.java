package lib.shm.ipc.result;

public final class IpcBooleanResult extends IpcBaseResult {
    private final boolean ok;

    private IpcBooleanResult(IpcError error, boolean ok) {
        super(error);
        this.ok = ok;
    }

    public static IpcBooleanResult error(IpcError error) {
        return new IpcBooleanResult(error, false);
    }

    public static IpcBooleanResult ok() {
        return new IpcBooleanResult(null, true);
    }

    public boolean isOk() {
        return ok;
    }
}