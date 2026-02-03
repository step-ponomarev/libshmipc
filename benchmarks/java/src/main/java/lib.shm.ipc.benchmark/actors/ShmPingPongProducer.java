package lib.shm.ipc.benchmark.actors;

import lib.shm.ipc.benchmark.SharedMemoryFile;
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
        SharedMemoryFile inShm = null;
        SharedMemoryFile outShm = null;

        IpcChannel inChannel = null;
        IpcChannel outChannel = null;

        final byte[] bytes = new byte[config.messageSize()];
        while (true) {
            final Signal signal = Signal.valueOf(inSignalChannel.read(SIGNAL_READ_TIMEOUT));
            if (signal == Signal.INIT || signal == Signal.STOP) {
                if (inShm != null) {
                    inShm.close();
                }

                if (outShm != null) {
                    outShm.close();
                }

                if (inChannel != null) {
                    inChannel.close();
                }

                if (outChannel != null) {
                    outChannel.close();
                }
            }

            if (signal == Signal.STOP) {
                outSignalChannel.write(Signal.DONE.bytes());
                break;
            }

            switch (signal) {
                case INIT:
                    inShm = SharedMemoryFile.create(Path.of(DATA_BUFFER_PREFIX + ".in"), suggestedSize);
                    outShm = SharedMemoryFile.create(Path.of(DATA_BUFFER_PREFIX + ".out"), suggestedSize);

                    inChannel = IpcChannel.create(inShm.segment(), inShm.size());
                    outChannel = IpcChannel.create(outShm.segment(), outShm.size());
                    break;

                case WARMUP:
                    for (int i = 0; i < config.warmupCount(); i++) {
                        pingPong(bytes, inChannel, outChannel);
                    }
                    break;
                case MEASURE:
                    final Histogram hist = new Histogram(1, Long.MAX_VALUE, 3);
                    for (int i = 0; i < config.messageCount(); i++) {
                        final long t0 = System.nanoTime();
                        pingPong(bytes, inChannel, outChannel);
                        long tn = System.nanoTime() - t0;
                        hist.recordValue(tn);
                    }

                    long p50 = hist.getValueAtPercentile(50.0);
                    System.out.println("Latency p50 %s".formatted(nsToUs(p50)));

                    long p95 = hist.getValueAtPercentile(95.0);
                    System.out.println("Latency p95 %s".formatted(nsToUs(p95)));

                    long p99 = hist.getValueAtPercentile(99.0);
                    System.out.println("Latency p99 %s".formatted(nsToUs(p99)));

                    long p9999 = hist.getValueAtPercentile(99.99);
                    System.out.println("Latency p99.99 %s".formatted(nsToUs(p9999)));

                    long max = hist.getMaxValue();
                    System.out.println("Latency max %s".formatted(nsToUs(max)));

                    long min = hist.getMinValue();
                    System.out.println("Latency min %s".formatted(nsToUs(min)));

                    System.out.println("Count " + hist.getTotalCount());

                    break;
                default:
                    throw new AssertionError("Unknown signal: " + signal);
            }
            outSignalChannel.write(Signal.DONE.bytes());
        }
    }

    static String nsToUs(long ns) {
        return String.format("%.3f µs", ns / 1_000.0);
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
