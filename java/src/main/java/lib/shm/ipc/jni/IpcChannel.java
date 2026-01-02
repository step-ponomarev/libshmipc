package lib.shm.ipc.jni;

import lib.shm.ipc.LibLoader;
import lib.shm.ipc.result.IpcBooleanResult;
import lib.shm.ipc.result.IpcBytesResult;
import lib.shm.ipc.result.IpcChannelResult;
import lib.shm.ipc.result.IpcError;

public class IpcChannel {
    static {
        LibLoader.load();
    }

    private final long channelAddress;
    private final long pollingAddress;

    private IpcChannel(long channelAddress, long pollingAddress) {
        this.channelAddress = channelAddress;
        this.pollingAddress = pollingAddress;
    }

    public native static IpcChannelResult create(String fileName, long sizeBytes);

    public native static IpcChannelResult attach(String fileName, long sizeBytes);

    public static native long suggestSize(long desiredCapacity);

    public IpcBytesResult read() {
        IpcBytesResult ipcBytesResult;

        do {
            ipcBytesResult = tryRead();
        } while (ipcBytesResult.isError());

        return ipcBytesResult;
    }

    public native IpcBooleanResult write(byte[] data);

    public native IpcBytesResult tryRead();

//    @Override
//    public native void close() throws IOException;
}
