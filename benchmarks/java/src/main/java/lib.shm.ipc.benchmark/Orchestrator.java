package lib.shm.ipc.benchmark;

import lib.shm.ipc.benchmark.signal.Signal;
import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.*;

import java.nio.file.Path;
import java.time.Duration;

public final class Orchestrator {
    private static final Duration TIMEOUT = Duration.ofSeconds(500);
    private static final String RUNNER_NAME = Runner.class.getName();
    public static final String CONTROL_BUFFER_PREFIX = "/tmp/ipc_signal_benchmark";
    private static final long SIGNAL_BUFFER_SIZE = 256;

    static void main(String[] args) throws Exception {
        final long suggestedSize = IpcChannel.getSuggestedSize(SIGNAL_BUFFER_SIZE);
        try (
                final SharedMemoryFile producerRequestShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".producer.in"), suggestedSize);
                final SharedMemoryFile producerResponseShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".producer.out"), suggestedSize);
                final IpcChannel producerRequestChannel = IpcChannel.create(producerRequestShm.segment(), suggestedSize);
                final IpcChannel producerResponseChannel = IpcChannel.create(producerResponseShm.segment(), suggestedSize);

                final SharedMemoryFile consumerRequestShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".consumer.in"), suggestedSize);
                final SharedMemoryFile consumerResponseShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".consumer.out"), suggestedSize);
                final IpcChannel consumerRequestChannel = IpcChannel.create(consumerRequestShm.segment(), suggestedSize);
                final IpcChannel consumerResponseChannel = IpcChannel.create(consumerResponseShm.segment(), suggestedSize);
        ) {
            final String javaBin = System.getProperty("java.home") + "/bin/java";
            final String classpath = System.getProperty("java.class.path");

            final Process producer = new ProcessBuilder(
                    javaBin,
                    "-cp", classpath,
                    RUNNER_NAME,
                    "--role=%s".formatted(Role.SHM_PRODUCER.role)
            ).redirectOutput(ProcessBuilder.Redirect.INHERIT)
                    .redirectErrorStream(true)
                    .start();

            final Process consumer = new ProcessBuilder(
                    javaBin,
                    "-cp", classpath,
                    RUNNER_NAME,
                    "--role=%s".formatted(Role.SHM_CONSUMER.role)
            ).redirectOutput(ProcessBuilder.Redirect.INHERIT)
                    .redirectErrorStream(true)
                    .start();

            Runtime.getRuntime().addShutdownHook(new Thread(() -> {
                producer.destroy();
                consumer.destroy();
            }));

            waitReady(producerResponseChannel);
            waitReady(consumerResponseChannel);

            System.out.println("--- Initialization --- ");
            sendSignal(producerRequestChannel, Signal.INIT);
            waitReady(producerResponseChannel);
            System.out.println("Producer initialization complete.");

            sendSignal(consumerRequestChannel, Signal.INIT);
            waitReady(consumerResponseChannel);
            System.out.println("Consumer initialization complete.");

            System.out.println("--- Warmup --- ");
            sendSignal(producerRequestChannel, Signal.WARMUP);
            sendSignal(consumerRequestChannel, Signal.WARMUP);
            waitReady(producerResponseChannel);
            waitReady(consumerResponseChannel);
            System.out.println("Warmup complete.");

            System.out.println("--- Measure --- ");
            sendSignal(producerRequestChannel, Signal.MEASURE);
            sendSignal(consumerRequestChannel, Signal.MEASURE);
            waitReady(producerResponseChannel);
            waitReady(consumerResponseChannel);
            System.out.println("Measure complete.");

            sendSignal(producerRequestChannel, Signal.STOP);
            waitReady(producerResponseChannel);

            sendSignal(consumerRequestChannel, Signal.STOP);
            waitReady(consumerResponseChannel);
        }
    }

    private static void waitReady(IpcChannel channel) throws IpcTimeoutException, IpcReadException {
        Signal signal = Signal.valueOf(channel.read(TIMEOUT));
        if (signal == Signal.DONE || signal == Signal.READY) {
            return;
        }

        throw new IllegalStateException("Unexpected signal: " + signal);
    }

    private static void sendSignal(IpcChannel channel, Signal signal) throws IpcLockedException, IpcWriteException, IpcSystemError {
        channel.write(signal.bytes());
    }
}
