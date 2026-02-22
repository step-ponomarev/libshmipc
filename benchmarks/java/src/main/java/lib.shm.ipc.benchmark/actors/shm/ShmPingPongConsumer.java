package lib.shm.ipc.benchmark.actors.shm;

import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.channel.IpcChannel;

import java.nio.file.Path;

public final class ShmPingPongConsumer extends ShmBenchmarkActor {
    public ShmPingPongConsumer(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {
        inShm = SharedMemoryFile.open(Path.of(DATA_BUFFER_PREFIX + ".in"));
        outShm = SharedMemoryFile.open(Path.of(DATA_BUFFER_PREFIX + ".out"));

        inChannel = IpcChannel.connect(inShm.segment());
        outChannel = IpcChannel.connect(outShm.segment());
    }

    @Override
    protected void onWarmup(ActorConfig config) throws Exception {
        for (int i = 0; i < config.warmupCount(); i++) {
            pingPong(inChannel, outChannel);
        }
    }

    @Override
    protected void onMeasure(ActorConfig config) throws Exception {
        for (int i = 0; i < config.messageCount(); i++) {
            pingPong(inChannel, outChannel);
        }
    }

    private static void pingPong(IpcChannel inChannel, IpcChannel outChannel) throws Exception {
        byte[] read = inChannel.read(DATA_READ_TIMEOUT);
        outChannel.write(read);
    }
}
