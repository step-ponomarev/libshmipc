package lib.shm.ipc.jni;

import lib.shm.ipc.LibLoader;
import lib.shm.ipc.conf.IpcChannelConfiguration;

public class IpcChannel {
    static {
        LibLoader.load();
    }

    private final long channelAddress;

    public IpcChannel(String fileName, long sizeBytes, IpcChannelConfiguration ipcChannelConfiguration) {
        this.channelAddress = init(fileName, sizeBytes, ipcChannelConfiguration);
        System.out.println(this.channelAddress);
    }

    public native void write(byte[] data);

    public native byte[] read();

    private native long init(String fileName, long sizeBytes, IpcChannelConfiguration ipcChannelConfiguration);

//    @Override
//    public native void close() throws IOException;
}
