package lib.shm.ipc.benchmark.actors.shm;

import ipc.channel.IpcChannel;
import ipc.status.IpcStatus;
import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.utils.PathUtils;

public final class ShmPingPongConsumer extends ShmBenchmarkActor {
    public ShmPingPongConsumer(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {
        inShm = SharedMemoryFile.open(PathUtils.inPath(DATA_BUFFER_PATH));
        outShm = SharedMemoryFile.open(PathUtils.outPath(DATA_BUFFER_PATH));

        inChannel = IpcChannel.attach(inShm.segment());
        outChannel = IpcChannel.attach(outShm.segment());
    }

    @Override
    protected void onWarmup(ActorConfig config) {
        for (int i = 0; i < config.warmupCount(); i++) {
            pingPong(inChannel, outChannel);
        }
    }

    @Override
    protected void onMeasure(ActorConfig config) {
        for (int i = 0; i < config.messageCount(); i++) {
            pingPong(inChannel, outChannel);
        }
    }

    private static void pingPong(IpcChannel inChannel, IpcChannel outChannel) {
        byte[] bytes;
        while ((bytes = inChannel.read(DATA_READ_TIMEOUT)) == null);
        while (outChannel.write(bytes) != IpcStatus.IPC_STATUS_OK);
    }
}
