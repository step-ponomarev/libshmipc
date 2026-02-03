package lib.shm.ipc.benchmark.actors;

import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.signal.Signal;
import lib.shm.ipc.channel.IpcChannel;

import java.nio.file.Path;

public final class ShmPingPongConsumer extends BenchmarkActor {

    @Override
    protected void run(ActorConfig config, IpcChannel inSignalChannel, IpcChannel outSignalChannel) throws Exception {
        outSignalChannel.write(Signal.DONE.bytes());
        SharedMemoryFile inShm = null;
        SharedMemoryFile outShm = null;

        IpcChannel inChannel = null;
        IpcChannel outChannel = null;

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
                    inShm = SharedMemoryFile.open(Path.of(DATA_BUFFER_PREFIX + ".in"));
                    outShm = SharedMemoryFile.open(Path.of(DATA_BUFFER_PREFIX + ".out"));

                    inChannel = IpcChannel.connect(inShm.segment());
                    outChannel = IpcChannel.connect(outShm.segment());
                    break;

                case WARMUP:
                    for (int i = 0; i < config.warmupCount(); i++) {
                        pingPong(inChannel, outChannel);
                    }
                    break;
                case MEASURE:
                    for (int i = 0; i < config.messageCount(); i++) {
                        pingPong(inChannel, outChannel);
                    }
                    break;
                default:
                    throw new AssertionError("Unknown signal: " + signal);
            }
            outSignalChannel.write(Signal.DONE.bytes());
        }
    }

    @Override
    protected String getSignalSuffix(String base) {
        return base + ".consumer";
    }

    private static void pingPong(IpcChannel inChannel, IpcChannel outChannel) throws Exception {
        byte[] read = inChannel.read(DATA_READ_TIMEOUT);
        outChannel.write(read);
    }
}
