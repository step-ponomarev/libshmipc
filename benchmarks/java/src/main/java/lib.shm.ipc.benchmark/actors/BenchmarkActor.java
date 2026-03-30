package lib.shm.ipc.benchmark.actors;

import ipc.channel.IpcChannel;
import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.signal.Signal;
import lib.shm.ipc.benchmark.utils.PathUtils;
import org.apache.commons.lang3.StringUtils;

import java.time.Duration;

public abstract class BenchmarkActor {
    protected static final Duration SIGNAL_READ_TIMEOUT = Duration.ofSeconds(60 * 3);
    protected static final Duration DATA_READ_TIMEOUT = Duration.ofSeconds(5);

    private final String pathSuffix;

    protected BenchmarkActor(String pathSuffix) {
        if (StringUtils.isEmpty(pathSuffix)) {
            throw new IllegalArgumentException("pathSuffix is null or empty");
        }

        this.pathSuffix = pathSuffix;
    }

    public final void run(ActorConfig config) throws Exception {
        try (final SharedMemoryFile inShm = SharedMemoryFile.open(PathUtils.inPath(pathSuffix));
             final SharedMemoryFile outShm = SharedMemoryFile.open(PathUtils.outPath(pathSuffix));
             final IpcChannel inChannel = IpcChannel.attach(inShm.segment());
             final IpcChannel outChannel = IpcChannel.attach(outShm.segment())
        ) {
            boolean running = true;
            while (running) {
                final Signal signal = Signal.valueOf(inChannel.read(SIGNAL_READ_TIMEOUT));
                if (signal == Signal.RESULT) {
                    outChannel.write(onResult(config));
                    continue;
                }

                switch (signal) {
                    case INIT -> onInit(config);
                    case HANDSHAKE -> onHandshake(config);
                    case WARMUP -> onWarmup(config);
                    case MEASURE -> onMeasure(config);
                    case STOP -> {
                        onStop(config);
                        running = false;
                    }
                }
                outChannel.write(Signal.DONE.bytes());
            }
        }
    }

    protected void onInit(ActorConfig config) throws Exception {
        throw new UnsupportedOperationException("Unsupported operation");
    }

    protected void onHandshake(ActorConfig config) throws Exception {
        throw new UnsupportedOperationException("Unsupported operation");
    }

    protected void onStop(ActorConfig config) throws Exception {
        throw new UnsupportedOperationException("Unsupported operation");
    }

    protected void onWarmup(ActorConfig config) throws Exception {
        throw new UnsupportedOperationException("Unsupported operation");
    }

    protected void onMeasure(ActorConfig config) throws Exception {
        throw new UnsupportedOperationException("Unsupported operation");
    }

    protected byte[] onResult(ActorConfig config) throws Exception {
        throw new UnsupportedOperationException("Unsupported operation");
    }
}
