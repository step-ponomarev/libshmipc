package lib.shm.ipc.jni;

import lib.shm.ipc.LibLoader;
import lib.shm.ipc.result.IpcBooleanResult;
import lib.shm.ipc.result.IpcBytesResult;
import lib.shm.ipc.result.IpcLongResult;

public class IpcChannel {
    static {
        LibLoader.load();
    }

    private final long channelAddress;

    // firstly addres
    private IpcChannel(long channelAddress) {
        this.channelAddress = channelAddress;

        long pollingAddress = getPollingAddress();
    }

    public static IpcChannel create(String fileName, long sizeBytes) {
        //TODO: handle error

        IpcLongResult init = init(fileName, sizeBytes, true);

        return new IpcChannel(init.getResult());
    }

    public static IpcChannel attach(String fileName, long sizeBytes) {
        //TODO: handle error
        IpcLongResult init = init(fileName, sizeBytes, false);

        return new IpcChannel(init.getResult());
    }

    public static native long suggestSize(long desiredCapacity);

    public IpcBytesResult read() {
        throw new UnsupportedOperationException();
    }

    public native IpcBooleanResult write(byte[] data);

    public native IpcBytesResult tryRead();

    private static native IpcLongResult init(String fileName, long sizeBytes, boolean create);

    private native long getPollingAddress();

//    @Override
//    public native void close() throws IOException;
}
