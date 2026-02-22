package lib.shm.ipc.benchmark.actors.shm;

import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.LatencyResult;
import lib.shm.ipc.benchmark.utils.PathUtils;
import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.IpcWriteException;
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
        inShm = SharedMemoryFile.create(PathUtils.inPath(DATA_BUFFER_PREFIX), suggestedSize);
        outShm = SharedMemoryFile.create(PathUtils.outPath(DATA_BUFFER_PREFIX), suggestedSize);

        inChannel = IpcChannel.create(inShm.segment(), inShm.size());
        outChannel = IpcChannel.create(outShm.segment(), outShm.size());

        hist = new Histogram(1, 60_000_000_000L, 3);
    }

    @Override
    protected void onWarmup(ActorConfig config) throws Exception {
        for (int i = 0; i < config.warmupCount(); i++) {
            pingPong(message, inChannel, outChannel);
        }
    }

    @Override
    protected void onMeasure(ActorConfig config) throws Exception {
        for (int i = 0; i < config.messageCount(); i++) {
            final long t0 = System.nanoTime();
            pingPong(message, inChannel, outChannel);
            long tn = System.nanoTime() - t0;
            hist.recordValue(tn);
        }
    }

    @Override
    protected byte[] onResult(ActorConfig config) {
        return new LatencyResult(
                nsToUs(hist.getValueAtPercentile(50.0)),
                nsToUs(hist.getValueAtPercentile(95.0)),
                nsToUs(hist.getValueAtPercentile(99.0)),
                nsToUs(hist.getValueAtPercentile(99.99)),
                nsToUs(hist.getMinValue()),
                nsToUs(hist.getMaxValue())
        ).serialize();
    }

    private static double nsToUs(long ns) {
        return (float) ns / 1_000.0;
    }

    private static void pingPong(byte[] bytes, IpcChannel inChannel, IpcChannel outChannel) throws Exception {
        while (true) {
            try {
                inChannel.write(bytes);
                outChannel.read(DATA_READ_TIMEOUT);
                break;
            } catch (IpcWriteException e) {
                Thread.onSpinWait();
            }
        }
    }
}
