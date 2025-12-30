package lib.shm.ipc.result;

public final class IpcLongResult extends IpcBaseResult {
    private final long result;

    private IpcLongResult(IpcError error, long result) {
        super(error);
        this.result = result;
    }

    public static IpcLongResult error(IpcError error) {
        return new IpcLongResult(error, 0);
    }

    public static IpcLongResult ok(long address) {
        return new IpcLongResult(null, address);
    }

    public long getResult() {
        return result;
    }
}
