package lib.shm.ipc.benchmark.actors;

import lib.shm.ipc.benchmark.Orchestrator;
import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.channel.IpcChannel;

import java.nio.file.Path;
import java.time.Duration;

public abstract class BenchmarkActor {
    protected static final Duration SIGNAL_READ_TIMEOUT = Duration.ofSeconds(60 * 3);
    protected static final Duration DATA_READ_TIMEOUT = Duration.ofSeconds(10);

    protected final static String DATA_BUFFER_PREFIX = "/tmp/ipc_data_benchmark";

    public final void run(ActorConfig config) throws Exception {
        try (
                final SharedMemoryFile inShm = SharedMemoryFile.open(Path.of(getSignalSuffix(Orchestrator.CONTROL_BUFFER_PREFIX) + ".in"));
                final SharedMemoryFile outShm = SharedMemoryFile.open(Path.of(getSignalSuffix(Orchestrator.CONTROL_BUFFER_PREFIX) + ".out"));
                final IpcChannel inChannel = IpcChannel.connect(inShm.segment());
                final IpcChannel outChannel = IpcChannel.connect(outShm.segment());
        ) {
            run(
                    config,
                    inChannel,
                    outChannel
            );
        }
    }

    protected abstract void run(ActorConfig config, IpcChannel inSignalChannel, IpcChannel outSignalChannel) throws Exception;

    protected abstract String getSignalSuffix(String base);
}
