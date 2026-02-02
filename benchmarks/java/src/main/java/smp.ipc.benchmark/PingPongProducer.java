package lib.shm.ipc.benchmark;

import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.IpcWriteException;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Random;

/**
 * Ping-pong producer for accurate latency measurement.
 * Sends a message and waits for response before sending next.
 *
 * Uses 2 channels: requestChannel (producer→consumer) and responseChannel (consumer→producer)
 *
 * Usage: java PingPongProducer <shm_path> <message_count> <warmup_count> <message_size_or_sizes>
 */
public class PingPongProducer {
    private static final Duration READ_TIMEOUT = Duration.ofSeconds(5);

    public static void main(String[] args) throws Exception {
        if (args.length < 4) {
            System.err.println("Usage: PingPongProducer <shm_path> <message_count> <warmup_count> <message_size_or_sizes>");
            System.exit(1);
        }

        Path shmPath = Path.of(args[0]);
        int messageCount = Integer.parseInt(args[1]);
        int warmupCount = Integer.parseInt(args[2]);
        int[] messageSizes = BenchmarkUtils.parseSizes(args[3]);
        int maxMessageSize = BenchmarkUtils.maxSize(messageSizes);

        Path requestShmPath = Path.of(shmPath + ".request");
        Path responseShmPath = Path.of(shmPath + ".response");

        long channelSize = IpcChannel.getSuggestedSize(maxMessageSize * 16L);

        System.out.printf("PingPong Producer starting: messages=%d, warmup=%d, sizes=%s bytes%n",
                messageCount, warmupCount, BenchmarkUtils.sizesToString(messageSizes));

        try (SharedMemoryFile requestShm = SharedMemoryFile.create(requestShmPath, channelSize);
             SharedMemoryFile responseShm = SharedMemoryFile.create(responseShmPath, channelSize)) {

            IpcChannel requestChannel = IpcChannel.create(requestShm.segment(), requestShm.size());
            IpcChannel responseChannel = IpcChannel.create(responseShm.segment(), responseShm.size());

            Path readyFile = Path.of(shmPath + ".ready");
            Files.writeString(readyFile, "ready");

            System.out.println("Waiting for consumer...");
            Path consumerReady = Path.of(shmPath + ".consumer_ready");
            while (!Files.exists(consumerReady)) {
                Thread.sleep(10);
            }

            System.out.println("Consumer connected. Starting ping-pong...");

            Arena payloadArena = Arena.ofShared();
            MemorySegment payload = payloadArena.allocate(maxMessageSize);
            Random random = new Random();

            System.out.println("Warmup phase: " + warmupCount + " round-trips");
            for (int i = 0; i < warmupCount; i++) {
                int messageSize = messageSizes[random.nextInt(messageSizes.length)];
                payload.set(ValueLayout.JAVA_LONG, 0, System.nanoTime());
                writeWithBackpressure(requestChannel, payload, messageSize);
                waitForResponse(responseChannel);
            }

            System.out.println("Measurement phase: " + messageCount + " round-trips");

            long minLatency = Long.MAX_VALUE;
            long maxLatency = Long.MIN_VALUE;
            long sumLatency = 0;
            long[] latencies = new long[messageCount];
            long totalBytes = 0;
            long benchStart = System.nanoTime();

            for (int i = 0; i < messageCount; i++) {
                int messageSize = messageSizes[random.nextInt(messageSizes.length)];
                long sendTime = System.nanoTime();
                payload.set(ValueLayout.JAVA_LONG, 0, sendTime);
                payload.set(ValueLayout.JAVA_INT, 8, i);
                writeWithBackpressure(requestChannel, payload, messageSize);

                int responseSeq = waitForResponse(responseChannel);
                long receiveTime = System.nanoTime();

                if (responseSeq != i) {
                    throw new RuntimeException("Sequence mismatch: expected " + i + ", got " + responseSeq);
                }

                long latency = receiveTime - sendTime;
                minLatency = Math.min(minLatency, latency);
                maxLatency = Math.max(maxLatency, latency);
                sumLatency += latency;
                latencies[i] = latency;
                totalBytes += (long) messageSize * 2;
            }

            long benchEnd = System.nanoTime();
            double durationMs = (benchEnd - benchStart) / 1_000_000.0;
            double throughput = messageCount / (durationMs / 1000.0);
            double avgLatency = (double) sumLatency / messageCount;
            double p50 = BenchmarkUtils.percentile(latencies, 50);
            double p90 = BenchmarkUtils.percentile(latencies, 90);
            double p99 = BenchmarkUtils.percentile(latencies, 99);

            Path donePath = Path.of(shmPath + ".producer_done");
            Files.writeString(donePath, "done");

            double bytesPerSec = totalBytes / (durationMs / 1000.0);
            double mbPerSec = bytesPerSec / (1024 * 1024);

            System.out.println();
            System.out.println("=== SHM Ping-Pong Latency (round-trip) ===");
            System.out.printf("  Min:    %,d ns (%.2f µs)%n", minLatency, minLatency / 1000.0);
            System.out.printf("  Avg:    %,.0f ns (%.2f µs)%n", avgLatency, avgLatency / 1000.0);
            System.out.printf("  Max:    %,d ns (%.2f µs)%n", maxLatency, maxLatency / 1000.0);
            System.out.printf("  P50:    %,.0f ns (%.2f µs)%n", p50, p50 / 1000.0);
            System.out.printf("  P90:    %,.0f ns (%.2f µs)%n", p90, p90 / 1000.0);
            System.out.printf("  P99:    %,.0f ns (%.2f µs)%n", p99, p99 / 1000.0);
            System.out.println();
            System.out.printf("  Duration:     %.2f ms%n", durationMs);
            System.out.printf("  Throughput:   %,.0f rt/sec%n", throughput);
            System.out.printf("  Bandwidth:    %,.2f MB/sec%n", mbPerSec);

            Path consumerDone = Path.of(shmPath + ".consumer_done");
            while (!Files.exists(consumerDone)) {
                Thread.sleep(10);
            }
            Files.deleteIfExists(readyFile);
            Files.deleteIfExists(consumerReady);
            Files.deleteIfExists(donePath);
            Files.deleteIfExists(consumerDone);
        }
    }

    private static void writeWithBackpressure(IpcChannel channel, MemorySegment payload, int size) throws Exception {
        while (true) {
            try {
                channel.write(payload, size);
                return;
            } catch (IpcWriteException e) {
                Thread.onSpinWait();
            }
        }
    }

    private static int waitForResponse(IpcChannel channel) throws Exception {
        byte[] response = channel.read(READ_TIMEOUT);
        return ByteBuffer.wrap(response).order(ByteOrder.nativeOrder()).getInt(0);
    }

}
