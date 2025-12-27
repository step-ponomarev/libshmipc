package lib.shm.ipc.jni;

import lib.shm.ipc.LibLoader;

public class IpcChannel {
    static {
        LibLoader.load();
    }

    private final long channelAddress;

    public IpcChannel(String fileName, long sizeBytes, boolean isProducer) {
        this.channelAddress = init(fileName, sizeBytes, isProducer);
        System.out.println(this.channelAddress);
    }

    public native void write(byte[] data);

    public native byte[] read();

    private native long init(String fileName, long sizeBytes, boolean isProducer);

//    @Override
//    public native void close() throws IOException;
}
