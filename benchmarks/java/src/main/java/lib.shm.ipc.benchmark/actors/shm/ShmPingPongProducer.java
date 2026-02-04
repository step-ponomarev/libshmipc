package lib.shm.ipc.benchmark.actors.shm;

import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.BenchmarkActor;
import lib.shm.ipc.benchmark.actors.LatencyResult;
import lib.shm.ipc.benchmark.signal.Signal;
import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.IpcWriteException;
import org.HdrHistogram.Histogram;

import java.nio.file.Path;

public final class ShmPingPongProducer extends BenchmarkActor {
    @Override
    protected void run(ActorConfig config, IpcChannel inSignalChannel, IpcChannel outSignalChannel) throws Exception {
        outSignalChannel.write(Signal.DONE.bytes());

        final long suggestedSize = IpcChannel.getSuggestedSize(config.bufferSize());
        SharedMemoryFile inShm;
        SharedMemoryFile outShm;

        IpcChannel inChannel = null;
        IpcChannel outChannel = null;

        Histogram hist = null;
        final byte[] bytes = new byte[config.messageSize()];
        while (true) {
            final Signal signal = Signal.valueOf(inSignalChannel.read(SIGNAL_READ_TIMEOUT));
            if (signal == Signal.STOP) {
                if (hist != null) {
                    LatencyResult latencyResult = new LatencyResult(
                            nsToUs(hist.getValueAtPercentile(50.0)),
                            nsToUs(hist.getValueAtPercentile(95.0)),
                            nsToUs(hist.getValueAtPercentile(99.0)),
                            nsToUs(hist.getValueAtPercentile(99.99)),
                            nsToUs(hist.getMinValue()),
                            nsToUs(hist.getMaxValue())
                    );

                    outSignalChannel.write(latencyResult.serialize());
                }

                break;
            }

            switch (signal) {
                case INIT:
                    inShm = SharedMemoryFile.create(Path.of(DATA_BUFFER_PREFIX + ".in"), suggestedSize);
                    outShm = SharedMemoryFile.create(Path.of(DATA_BUFFER_PREFIX + ".out"), suggestedSize);

                    inChannel = IpcChannel.create(inShm.segment(), inShm.size());
                    outChannel = IpcChannel.create(outShm.segment(), outShm.size());
                    hist = new Histogram(1, 60_000_000_000L, 3);
                    break;
                case WARMUP:
                    for (int i = 0; i < config.warmupCount(); i++) {
                        pingPong(bytes, inChannel, outChannel);
                    }
                    break;
                case MEASURE:
                    for (int i = 0; i < config.messageCount(); i++) {
                        final long t0 = System.nanoTime();
                        pingPong(bytes, inChannel, outChannel);
                        long tn = System.nanoTime() - t0;
                        hist.recordValue(tn);
                    }
                    break;
                default:
                    throw new AssertionError("Unknown signal: " + signal);
            }
            outSignalChannel.write(Signal.DONE.bytes());
        }
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

    @Override
    protected String getSignalSuffix(String base) {
        return base + ".producer";
    }
}
