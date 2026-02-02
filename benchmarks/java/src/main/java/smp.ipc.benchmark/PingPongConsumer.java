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

/**
 * Ping-pong consumer for accurate latency measurement.
 * Receives a message and immediately sends response.
 *
 * Uses 2 channels: requestChannel (producer→consumer) and responseChannel (consumer→producer)
 *
 * Usage: java PingPongConsumer <shm_path> <message_count> <warmup_count> <message_size_or_sizes>
 */
public class PingPongConsumer {
    private static final Duration READ_TIMEOUT = Duration.ofMillis(200);

    static void main(String[] args) throws Exception {
        if (args.length < 4) {
            System.err.println("Usage: PingPongConsumer <shm_path> <message_count> <warmup_count> <message_size_or_sizes>");
            System.exit(1);
        }

        Path shmPath = Path.of(args[0]);
        int messageCount = Integer.parseInt(args[1]);
        int warmupCount = Integer.parseInt(args[2]);
        int[] messageSizes = BenchmarkUtils.parseSizes(args[3]);
        int maxMessageSize = BenchmarkUtils.maxSize(messageSizes);

        Path requestShmPath = Path.of(shmPath + ".request");
        Path responseShmPath = Path.of(shmPath + ".response");

        System.out.printf("PingPong Consumer starting: messages=%d, warmup=%d, sizes=%s bytes%n",
                messageCount, warmupCount, BenchmarkUtils.sizesToString(messageSizes));

        Path readyFile = Path.of(shmPath + ".ready");
        System.out.println("Waiting for producer...");
        while (!Files.exists(readyFile)) {
            Thread.sleep(10);
        }

        try (SharedMemoryFile requestShm = SharedMemoryFile.open(requestShmPath);
             SharedMemoryFile responseShm = SharedMemoryFile.open(responseShmPath);
             IpcChannel requestChannel = IpcChannel.connect(requestShm.segment());
             IpcChannel responseChannel = IpcChannel.connect(responseShm.segment());
        ) {
            Path consumerReady = Path.of(shmPath + ".consumer_ready");
            Files.writeString(consumerReady, "ready");

            System.out.println("Connected. Running ping-pong...");

            Arena payloadArena = Arena.ofShared();
            MemorySegment responsePayload = payloadArena.allocate(maxMessageSize);

            int totalMessages = warmupCount + messageCount;

            for (int i = 0; i < totalMessages; i++) {
                byte[] request = requestChannel.read(READ_TIMEOUT);
                int seq = ByteBuffer.wrap(request).order(ByteOrder.nativeOrder()).getInt(8);
                responsePayload.set(ValueLayout.JAVA_INT, 0, seq);

                writeWithBackpressure(responseChannel, responsePayload, request.length);
            }

            System.out.println("Consumer done.");

            Path producerDone = Path.of(shmPath + ".producer_done");
            while (!Files.exists(producerDone)) {
                Thread.sleep(10);
            }

            Path consumerDone = Path.of(shmPath + ".consumer_done");
            Files.writeString(consumerDone, "done");
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

}
