package lib.shm.ipc.result;

public final class IpcBytesResult extends IpcBaseResult {
    private final byte[] bytes;

    private IpcBytesResult(IpcError error, byte[] bytes) {
        super(error);
        this.bytes = bytes;
    }

    public static IpcBytesResult error(IpcError error) {
        return new IpcBytesResult(error, null);
    }

    public static IpcBytesResult ok(byte[] bytes) {
        return new IpcBytesResult(null, bytes);
    }

    public byte[] getBytes() {
        return bytes;
    }
}
