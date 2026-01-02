package lib.shm.ipc.result;

import lib.shm.ipc.jni.IpcChannel;

public final class IpcChannelResult extends IpcBaseResult {
    private final IpcChannel channel;

    private IpcChannelResult(IpcError error, IpcChannel channel) {
        super(error);
        this.channel = channel;
    }

    public static IpcChannelResult error(IpcError error) {
        return new IpcChannelResult(error, null);
    }

    public static IpcChannelResult ok(IpcChannel channel) {
        return new IpcChannelResult(null, channel);
    }

    public IpcChannel getChannel() {
        return channel;
    }
}
