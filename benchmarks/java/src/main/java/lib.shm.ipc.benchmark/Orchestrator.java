package lib.shm.ipc.benchmark;

import ipc.channel.IpcChannel;
import lib.shm.ipc.benchmark.actors.LatencyResult;
import lib.shm.ipc.benchmark.actors.Mode;
import lib.shm.ipc.benchmark.args.ArgsUtils;
import lib.shm.ipc.benchmark.signal.Signal;
import lib.shm.ipc.benchmark.utils.PathUtils;

import java.time.Duration;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public final class Orchestrator {
    private static final Duration TIMEOUT = Duration.ofSeconds(500);
    private static final String RUNNER_NAME = Runner.class.getName();
    private static final long SIGNAL_BUFFER_SIZE = 256;

    static void main(String[] args) throws Exception {
        final Map<String, String> params = ArgsUtils.getArgs(args);
        final Mode mode = params.containsKey(ArgsUtils.ARG_MODE)
                ? Mode.valueOf(params.get(ArgsUtils.ARG_MODE))
                : Mode.SHM_PING_PONG;

        final long suggestedSize = IpcChannel.getSuggestedSize(SIGNAL_BUFFER_SIZE);
        try (
                final SharedMemoryFile producerRequestShm = SharedMemoryFile.create(PathUtils.inPath(mode.producerRole), suggestedSize);
                final SharedMemoryFile producerResponseShm = SharedMemoryFile.create(PathUtils.outPath(mode.producerRole), suggestedSize);
                final IpcChannel producerInChannel = IpcChannel.init(producerRequestShm.segment(), suggestedSize);
                final IpcChannel producerOutChannel = IpcChannel.init(producerResponseShm.segment(), suggestedSize);

                final SharedMemoryFile consumerRequestShm = SharedMemoryFile.create(PathUtils.inPath(mode.consumerRole), suggestedSize);
                final SharedMemoryFile consumerResponseShm = SharedMemoryFile.create(PathUtils.outPath(mode.consumerRole), suggestedSize);
                final IpcChannel consumerInChannel = IpcChannel.init(consumerRequestShm.segment(), suggestedSize);
                final IpcChannel consumerOutChannel = IpcChannel.init(consumerResponseShm.segment(), suggestedSize);
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
            baseArgs.add(1, "--enable-native-access=ALL-UNNAMED");
            baseArgs.add(2, "-cp");
            baseArgs.add(3, classpath);
            baseArgs.add(4, RUNNER_NAME);

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
                case SHM_PING_PONG ->
                        runShmLatencyBenchmark(producerInChannel, producerOutChannel, consumerInChannel, consumerOutChannel);
                case UDS_PING_PONG ->
                        runUdsLatencyBenchmark(producerInChannel, producerOutChannel, consumerInChannel, consumerOutChannel);
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
        preparedArgs.putIfAbsent(ArgsUtils.ARG_MESSAGE_SIZE, String.valueOf(64)); // 64kb
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
    ) {
        sendSignal(producerInChannel, Signal.INIT);
        waitReady(producerOutChannel);

        sendSignal(consumerInChannel, Signal.INIT);
        waitReady(consumerOutChannel);

        sendSignal(producerInChannel, Signal.WARMUP);
        sendSignal(consumerInChannel, Signal.WARMUP);
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);

        sendSignal(producerInChannel, Signal.MEASURE);
        sendSignal(consumerInChannel, Signal.MEASURE);
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);

        sendSignal(producerInChannel, Signal.RESULT);
        printResult(LatencyResult.deserialize(
                producerOutChannel.read(TIMEOUT)
        ));

        sendSignal(producerInChannel, Signal.STOP);
        waitReady(producerOutChannel);

        sendSignal(consumerInChannel, Signal.STOP);
        waitReady(consumerOutChannel);
    }

    private static void runUdsLatencyBenchmark(
            IpcChannel producerInChannel,
            IpcChannel producerOutChannel,
            IpcChannel consumerInChannel,
            IpcChannel consumerOutChannel
    ) {
        sendSignal(consumerInChannel, Signal.INIT);
        waitReady(consumerOutChannel);

        sendSignal(producerInChannel, Signal.INIT);
        waitReady(producerOutChannel);

        sendSignal(consumerInChannel, Signal.HANDSHAKE);
        sendSignal(producerInChannel, Signal.HANDSHAKE);
        waitReady(consumerOutChannel);
        waitReady(producerOutChannel);

        sendSignal(producerInChannel, Signal.WARMUP);
        sendSignal(consumerInChannel, Signal.WARMUP);
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);

        sendSignal(producerInChannel, Signal.MEASURE);
        sendSignal(consumerInChannel, Signal.MEASURE);
        waitReady(producerOutChannel);
        waitReady(consumerOutChannel);

        sendSignal(producerInChannel, Signal.RESULT);
        printResult(LatencyResult.deserialize(
                producerOutChannel.read(TIMEOUT)
        ));

        sendSignal(producerInChannel, Signal.STOP);
        waitReady(producerOutChannel);

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

    private static void waitReady(IpcChannel channel) {
        Signal signal = Signal.valueOf(channel.read(TIMEOUT));
        if (signal == Signal.DONE) {
            return;
        }

        throw new IllegalStateException("Unexpected signal: " + signal);
    }

    private static void sendSignal(IpcChannel channel, Signal signal) {
        channel.write(signal.bytes());
    }
}
