package lib.shm.ipc.benchmark.actors.shm;

import ipc.channel.IpcChannel;
import ipc.status.IpcStatus;
import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.utils.PathUtils;
import lib.shm.ipc.benchmark.utils.HistogramUtils;
import org.HdrHistogram.Histogram;

public final class ShmPingPongProducer extends ShmBenchmarkActor {
    private Histogram hist;
    byte[] message;

    public ShmPingPongProducer(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {
        message = new byte[config.messageSize()];

        final long suggestedSize = IpcChannel.getSuggestedSize(config.bufferSize());
        inShm = SharedMemoryFile.create(PathUtils.inPath(DATA_BUFFER_PATH), suggestedSize);
        outShm = SharedMemoryFile.create(PathUtils.outPath(DATA_BUFFER_PATH), suggestedSize);

        inChannel = IpcChannel.init(inShm.segment(), inShm.size());
        outChannel = IpcChannel.init(outShm.segment(), outShm.size());

        hist = new Histogram(1, 60_000_000_000L, 3);
    }

    @Override
    protected void onWarmup(ActorConfig config) {
        for (int i = 0; i < config.warmupCount(); i++) {
            pingPong(message, inChannel, outChannel);
        }
    }

    @Override
    protected void onMeasure(ActorConfig config) {
        for (int i = 0; i < config.messageCount(); i++) {
            final long t0 = System.nanoTime();
            pingPong(message, inChannel, outChannel);
            long tn = System.nanoTime() - t0;
            hist.recordValue(tn);
        }
    }

    @Override
    protected byte[] onResult(ActorConfig config) {
        return HistogramUtils.toLatencyResult(hist).serialize();
    }

    private static void pingPong(byte[] bytes, IpcChannel inChannel, IpcChannel outChannel) {
        while (inChannel.write(bytes) != IpcStatus.IPC_STATUS_OK) ;
        while (outChannel.read(DATA_READ_TIMEOUT) == null) ;
    }
}
