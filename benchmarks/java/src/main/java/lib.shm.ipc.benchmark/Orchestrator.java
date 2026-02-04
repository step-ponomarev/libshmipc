package lib.shm.ipc.benchmark;

import lib.shm.ipc.benchmark.actors.LatencyResult;
import lib.shm.ipc.benchmark.args.ArgMode;
import lib.shm.ipc.benchmark.args.ArgsUtils;
import lib.shm.ipc.benchmark.signal.Signal;
import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.IpcLockedException;
import lib.shm.ipc.exeption.IpcReadException;
import lib.shm.ipc.exeption.IpcSystemError;
import lib.shm.ipc.exeption.IpcTimeoutException;
import lib.shm.ipc.exeption.IpcWriteException;

import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public final class Orchestrator {
    private static final Duration TIMEOUT = Duration.ofSeconds(500);
    private static final String RUNNER_NAME = Runner.class.getName();
    public static final String CONTROL_BUFFER_PREFIX = "/tmp/ipc_signal_benchmark";
    private static final long SIGNAL_BUFFER_SIZE = 256;

    static void main(String[] args) throws Exception {
        final Map<String, String> params = ArgsUtils.getArgs(args);
        final ArgMode mode = params.containsKey(ArgsUtils.ARG_MODE)
                ? ArgMode.valueOf(params.get(ArgsUtils.ARG_MODE))
                : ArgMode.SHM_LATENCY;

        final long suggestedSize = IpcChannel.getSuggestedSize(SIGNAL_BUFFER_SIZE);
        try (
                final SharedMemoryFile producerRequestShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".producer.in"), suggestedSize);
                final SharedMemoryFile producerResponseShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".producer.out"), suggestedSize);
                final IpcChannel producerInChannel = IpcChannel.create(producerRequestShm.segment(), suggestedSize);
                final IpcChannel producerOutChannel = IpcChannel.create(producerResponseShm.segment(), suggestedSize);

                final SharedMemoryFile consumerRequestShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".consumer.in"), suggestedSize);
                final SharedMemoryFile consumerResponseShm = SharedMemoryFile.create(Path.of(CONTROL_BUFFER_PREFIX + ".consumer.out"), suggestedSize);
                final IpcChannel consumerInChannel = IpcChannel.create(consumerRequestShm.segment(), suggestedSize);
                final IpcChannel consumerOutChannel = IpcChannel.create(consumerResponseShm.segment(), suggestedSize);
        ) {

            final Map<String, String> preparedArgs = prepareArgs(params);
            printArgs(preparedArgs);

            final String javaBin = System.getProperty("java.home") + "/bin/java";
            final String classpath = System.getProperty("java.class.path");
            final List<String> baseArgs = preparedArgs.entrySet()
                    .stream()
                    .map(e -> "--%s=%s".formatted(e.getKey(), e.getValue()))
                    .collect(Collectors.toCollection(ArrayList::new));
            baseArgs.add(0, javaBin);
            baseArgs.add(1, "-cp");
            baseArgs.add(2, classpath);
            baseArgs.add(3, RUNNER_NAME);

            //TODO: починитьь эту хуйню
            final List<String> producerArgs = new ArrayList<>(baseArgs);
            producerArgs.add("--%s=%s".formatted(ArgsUtils.ARG_ROLE, mode.producerRole));
            final Process producer = new ProcessBuilder(producerArgs
            ).redirectOutput(ProcessBuilder.Redirect.INHERIT)
                    .redirectErrorStream(true)
                    .start();

            final List<String> consumerArgs = new ArrayList<>(baseArgs);
            consumerArgs.add("--%s=%s".formatted(ArgsUtils.ARG_ROLE, mode.consumerRole));
            final Process consumer = new ProcessBuilder(consumerArgs)
                    .redirectOutput(ProcessBuilder.Redirect.INHERIT)
                    .redirectErrorStream(true)
                    .start();

            Runtime.getRuntime().addShutdownHook(new Thread(() -> {
                producer.destroy();
                consumer.destroy();
            }));

            switch (mode) {
                case SHM_LATENCY ->
                        runShmLatencyBenchmark(producerInChannel, producerOutChannel, consumerInChannel, consumerOutChannel);
                default -> throw new IllegalArgumentException(params.get(ArgsUtils.ARG_MODE) + " is not supported");
            }
        }
    }

    private static Map<String, String> prepareArgs(Map<String, String> args) {
        final Map<String, String> preparedArgs = new HashMap<>();
        for (Map.Entry<String, String> arg : args.entrySet()) {
            if (arg.getKey().equals(ArgsUtils.ARG_MODE)) {
                continue;
            }

            preparedArgs.put(arg.getKey(), arg.getValue());
        }

        preparedArgs.putIfAbsent(ArgsUtils.ARG_MESSAGE_COUNT, "1000000");
        preparedArgs.putIfAbsent(ArgsUtils.ARG_WARMUP_COUNT, "100000");
        preparedArgs.putIfAbsent(ArgsUtils.ARG_MESSAGE_SIZE, String.valueOf(64 * 1024)); // 64kb
        preparedArgs.putIfAbsent(
                ArgsUtils.ARG_BUFFER_SIZE,
                String.valueOf(Integer.parseInt(preparedArgs.get(ArgsUtils.ARG_MESSAGE_SIZE)) * 16) // 16 messages
        );

        return preparedArgs;
    }

    private static void runShmLatencyBenchmark(
            IpcChannel producerInChannel,
            IpcChannel producerOutChannel,
            IpcChannel consumerInChannel,
            IpcChannel consumerOutChannel
    ) throws IpcLockedException, IpcWriteException, IpcSystemError, IpcTimeoutException, IpcReadException {
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);

        System.out.println("--- Initialization ---");
        sendSignal(producerInChannel, Signal.INIT);
        waitReady(producerOutChannel);
        System.out.println("Producer initialization complete.");

        sendSignal(consumerInChannel, Signal.INIT);
        waitReady(consumerOutChannel);
        System.out.println("Consumer initialization complete.");
        System.out.println("------");

        System.out.println("--- Warmup --- "); // todo title
        sendSignal(producerInChannel, Signal.WARMUP);
        sendSignal(consumerInChannel, Signal.WARMUP);
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);
        System.out.println("Warmup complete.");
        System.out.println("------"); // todo end block

        System.out.println("--- Measure ---");
        sendSignal(producerInChannel, Signal.MEASURE);
        sendSignal(consumerInChannel, Signal.MEASURE);
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);
        System.out.println("Measure complete.");
        System.out.println("------");

        sendSignal(producerInChannel, Signal.STOP);
        printResult(LatencyResult.deserialize(
                producerOutChannel.read(TIMEOUT)
        ));

        sendSignal(consumerInChannel, Signal.STOP);
        waitReady(consumerOutChannel);
    }

    private static void printArgs(Map<String, String> args) {
        System.out.println("--- Configuration ---");
        for (String key : args.keySet()) {
            System.out.printf("%s: %s%n", key, args.get(key));
        }
        System.out.println("------"); // todo end block
    }

    private static void printResult(LatencyResult result) {
        System.out.println("--- Results ---");
        System.out.printf("Latency p50 %.3fus%n", result.p50Us());
        System.out.printf("Latency p95 %.3fus%n", result.p95Us());
        System.out.printf("Latency p99 %.3fus%n", result.p99Us());
        System.out.printf("Latency p99.99 %.3fus%n", result.p99_99Us());
        System.out.printf("Latency min %.3fus%n", result.minUs());
        System.out.printf("Latency max %.3fus%n", result.maxUs());
        System.out.println("------"); // todo end block
    }

    //TODO: stupid pice of sheeeet
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
