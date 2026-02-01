package lib.shm.ipc.benchmark;

import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.IpcWriteException;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.file.Path;

/**
 * Ping-pong producer for accurate latency measurement.
 * Sends a message and waits for response before sending next.
 *
 * Uses 2 channels: requestChannel (producer→consumer) and responseChannel (consumer→producer)
 *
 * Usage: java PingPongProducer <shm_path> <message_count> <warmup_count> <message_size>
 */
public class PingPongProducer {

    public static void main(String[] args) throws Exception {
        if (args.length < 4) {
            System.err.println("Usage: PingPongProducer <shm_path> <message_count> <warmup_count> <message_size>");
            System.exit(1);
        }

        Path shmPath = Path.of(args[0]);
        int messageCount = Integer.parseInt(args[1]);
        int warmupCount = Integer.parseInt(args[2]);
        int messageSize = Integer.parseInt(args[3]);

        if (messageSize < 8) {
            messageSize = 8;
        }

        // Two separate shm files for two channels
        Path requestShmPath = Path.of(shmPath + ".request");
        Path responseShmPath = Path.of(shmPath + ".response");

        long channelSize = IpcChannel.getSuggestedSize(1024 * 1024 * 1024); // Small buffer for backpressure

        System.out.printf("PingPong Producer starting: messages=%d, warmup=%d, size=%d bytes%n",
                messageCount, warmupCount, messageSize);

        try (SharedMemoryFile requestShm = SharedMemoryFile.create(requestShmPath, channelSize);
             SharedMemoryFile responseShm = SharedMemoryFile.create(responseShmPath, channelSize)) {

            IpcChannel requestChannel = IpcChannel.create(requestShm.segment(), requestShm.size());
            IpcChannel responseChannel = IpcChannel.create(responseShm.segment(), responseShm.size());

            // Signal ready
            Path readyFile = Path.of(shmPath + ".ready");
            java.nio.file.Files.writeString(readyFile, "ready");

            System.out.println("Waiting for consumer...");
            Path consumerReady = Path.of(shmPath + ".consumer_ready");
            while (!java.nio.file.Files.exists(consumerReady)) {
                Thread.sleep(10);
            }

            System.out.println("Consumer connected. Starting ping-pong...");

            Arena payloadArena = Arena.ofShared();
            MemorySegment payload = payloadArena.allocate(messageSize);

            // Warmup phase
            System.out.println("Warmup phase: " + warmupCount + " round-trips");
            for (int i = 0; i < warmupCount; i++) {
                payload.set(ValueLayout.JAVA_LONG, 0, System.nanoTime());
                writeWithBackpressure(requestChannel, payload, messageSize);
                waitForResponse(responseChannel);
            }

            System.out.println("Measurement phase: " + messageCount + " round-trips");

            long minLatency = Long.MAX_VALUE;
            long maxLatency = Long.MIN_VALUE;
            long sumLatency = 0;
            long benchStart = System.nanoTime();

            // Measurement phase - ping pong
            for (int i = 0; i < messageCount; i++) {
                long sendTime = System.nanoTime();
                payload.set(ValueLayout.JAVA_LONG, 0, sendTime);
                payload.set(ValueLayout.JAVA_INT, 8, i);  // sequence number
                writeWithBackpressure(requestChannel, payload, messageSize);

                int responseSeq = waitForResponse(responseChannel);
                long receiveTime = System.nanoTime();

                // Verify sequence
                if (responseSeq != i) {
                    throw new RuntimeException("Sequence mismatch: expected " + i + ", got " + responseSeq);
                }

                long latency = receiveTime - sendTime;
                minLatency = Math.min(minLatency, latency);
                maxLatency = Math.max(maxLatency, latency);
                sumLatency += latency;
            }

            long benchEnd = System.nanoTime();
            double durationMs = (benchEnd - benchStart) / 1_000_000.0;
            double throughput = messageCount / (durationMs / 1000.0);
            double avgLatency = (double) sumLatency / messageCount;

            // Signal done
            Path donePath = Path.of(shmPath + ".producer_done");
            java.nio.file.Files.writeString(donePath, "done");

            long totalBytes = (long) messageCount * messageSize * 2;  // request + response
            double bytesPerSec = totalBytes / (durationMs / 1000.0);
            double mbPerSec = bytesPerSec / (1024 * 1024);

            System.out.println();
            System.out.println("=== SHM Ping-Pong Latency (round-trip) ===");
            System.out.printf("  Min:    %,d ns (%.2f µs)%n", minLatency, minLatency / 1000.0);
            System.out.printf("  Avg:    %,.0f ns (%.2f µs)%n", avgLatency, avgLatency / 1000.0);
            System.out.printf("  Max:    %,d ns (%.2f µs)%n", maxLatency, maxLatency / 1000.0);
            System.out.println();
            System.out.printf("  Round-trips:  %,d%n", messageCount);
            System.out.printf("  Duration:     %.2f ms%n", durationMs);
            System.out.printf("  Throughput:   %,.0f rt/sec%n", throughput);
            System.out.printf("  Bandwidth:    %,.2f MB/sec%n", mbPerSec);

            // Cleanup
            Path consumerDone = Path.of(shmPath + ".consumer_done");
            while (!java.nio.file.Files.exists(consumerDone)) {
                Thread.sleep(10);
            }
            java.nio.file.Files.deleteIfExists(readyFile);
            java.nio.file.Files.deleteIfExists(consumerReady);
            java.nio.file.Files.deleteIfExists(donePath);
            java.nio.file.Files.deleteIfExists(consumerDone);
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
        while (true) {
            byte[] response = channel.tryRead();
            if (response != null) {
                // Extract sequence number from response
                return java.nio.ByteBuffer.wrap(response).order(java.nio.ByteOrder.nativeOrder()).getInt(0);
            }
            Thread.onSpinWait();
        }
    }
}
