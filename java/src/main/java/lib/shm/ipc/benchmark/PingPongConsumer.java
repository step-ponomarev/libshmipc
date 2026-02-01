package lib.shm.ipc.benchmark;

import lib.shm.ipc.channel.IpcChannel;
import lib.shm.ipc.exeption.IpcWriteException;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Path;

/**
 * Ping-pong consumer for accurate latency measurement.
 * Receives a message and immediately sends response.
 *
 * Uses 2 channels: requestChannel (producer→consumer) and responseChannel (consumer→producer)
 *
 * Usage: java PingPongConsumer <shm_path> <message_count> <warmup_count> <message_size>
 */
public class PingPongConsumer {

    public static void main(String[] args) throws Exception {
        if (args.length < 4) {
            System.err.println("Usage: PingPongConsumer <shm_path> <message_count> <warmup_count> <message_size>");
            System.exit(1);
        }

        Path shmPath = Path.of(args[0]);
        int messageCount = Integer.parseInt(args[1]);
        int warmupCount = Integer.parseInt(args[2]);
        int messageSize = Integer.parseInt(args[3]);

        if (messageSize < 8) {
            messageSize = 8;
        }

        Path requestShmPath = Path.of(shmPath + ".request");
        Path responseShmPath = Path.of(shmPath + ".response");

        System.out.printf("PingPong Consumer starting: messages=%d, warmup=%d, size=%d bytes%n",
                messageCount, warmupCount, messageSize);

        // Wait for producer to create shm
        Path readyFile = Path.of(shmPath + ".ready");
        System.out.println("Waiting for producer...");
        while (!java.nio.file.Files.exists(readyFile)) {
            Thread.sleep(10);
        }

        try (SharedMemoryFile requestShm = SharedMemoryFile.open(requestShmPath);
             SharedMemoryFile responseShm = SharedMemoryFile.open(responseShmPath)) {

            IpcChannel requestChannel = IpcChannel.connect(requestShm.segment());
            IpcChannel responseChannel = IpcChannel.connect(responseShm.segment());

            // Signal ready
            Path consumerReady = Path.of(shmPath + ".consumer_ready");
            java.nio.file.Files.writeString(consumerReady, "ready");

            System.out.println("Connected. Running ping-pong...");

            Arena payloadArena = Arena.ofShared();
            MemorySegment responsePayload = payloadArena.allocate(messageSize);

            int totalMessages = warmupCount + messageCount;

            // Echo loop: read request, send response with sequence
            for (int i = 0; i < totalMessages; i++) {
                // Wait for request
                byte[] request = null;
                while (request == null) {
                    request = requestChannel.tryRead();
                    if (request == null) {
                        Thread.onSpinWait();
                    }
                }

                // Extract sequence from request and put in response
                int seq = ByteBuffer.wrap(request).order(ByteOrder.nativeOrder()).getInt(8);
                responsePayload.set(ValueLayout.JAVA_INT, 0, seq);

                // Send response immediately
                writeWithBackpressure(responseChannel, responsePayload, messageSize);
            }

            System.out.println("Consumer done.");

            // Wait for producer done signal
            Path producerDone = Path.of(shmPath + ".producer_done");
            while (!java.nio.file.Files.exists(producerDone)) {
                Thread.sleep(10);
            }

            // Signal done
            Path consumerDone = Path.of(shmPath + ".consumer_done");
            java.nio.file.Files.writeString(consumerDone, "done");
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
