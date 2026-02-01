package lib.shm.ipc.benchmark;

import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.SocketChannel;
import java.nio.file.Path;

/**
 * Socket ping-pong client for accurate latency measurement.
 * Sends a message and waits for response before sending next.
 *
 * Usage: java SocketPingPongClient <mode> <host:port_or_path> <message_count> <warmup_count> <message_size>
 */
public class SocketPingPongClient {

    public static void main(String[] args) throws Exception {
        if (args.length < 5) {
            System.err.println("Usage: SocketPingPongClient <tcp|unix> <host:port_or_path> <message_count> <warmup_count> <message_size>");
            System.exit(1);
        }

        String mode = args[0];
        String target = args[1];
        int messageCount = Integer.parseInt(args[2]);
        int warmupCount = Integer.parseInt(args[3]);
        int messageSize = Integer.parseInt(args[4]);

        if (messageSize < 8) {
            messageSize = 8;
        }

        if ("tcp".equals(mode)) {
            String[] parts = target.split(":");
            String host = parts[0];
            int port = Integer.parseInt(parts[1]);
            runTcpClient(host, port, messageCount, warmupCount, messageSize);
        } else if ("unix".equals(mode)) {
            runUnixClient(Path.of(target), messageCount, warmupCount, messageSize);
        } else {
            System.err.println("Unknown mode: " + mode);
            System.exit(1);
        }
    }

    private static void runTcpClient(String host, int port, int messageCount, int warmupCount, int messageSize) throws Exception {
        System.out.printf("TCP PingPong Client connecting to %s:%d%n", host, port);

        try (SocketChannel channel = SocketChannel.open()) {
            channel.connect(new java.net.InetSocketAddress(host, port));
            channel.socket().setTcpNoDelay(true);
            runPingPong(channel, messageCount, warmupCount, messageSize);
        }
    }

    private static void runUnixClient(Path socketPath, int messageCount, int warmupCount, int messageSize) throws Exception {
        System.out.printf("Unix PingPong Client connecting to %s%n", socketPath);

        try (SocketChannel channel = SocketChannel.open(StandardProtocolFamily.UNIX)) {
            channel.connect(UnixDomainSocketAddress.of(socketPath));
            runPingPong(channel, messageCount, warmupCount, messageSize);
        }
    }

    private static void runPingPong(SocketChannel channel, int messageCount, int warmupCount, int messageSize) throws Exception {
        ByteBuffer buffer = ByteBuffer.allocateDirect(messageSize).order(ByteOrder.nativeOrder());

        // Warmup phase
        System.out.println("Warmup phase: " + warmupCount + " round-trips");
        for (int i = 0; i < warmupCount; i++) {
            buffer.clear();
            buffer.putLong(System.nanoTime());
            buffer.position(messageSize);
            buffer.flip();
            writeFully(channel, buffer);

            buffer.clear();
            readFully(channel, buffer);
        }

        System.out.println("Measurement phase: " + messageCount + " round-trips");

        long minLatency = Long.MAX_VALUE;
        long maxLatency = Long.MIN_VALUE;
        long sumLatency = 0;
        long benchStart = System.nanoTime();

        // Measurement phase - ping pong
        for (int i = 0; i < messageCount; i++) {
            long sendTime = System.nanoTime();

            buffer.clear();
            buffer.putLong(sendTime);
            buffer.putInt(i);  // sequence number
            buffer.position(messageSize);
            buffer.flip();
            writeFully(channel, buffer);

            buffer.clear();
            readFully(channel, buffer);
            buffer.flip();

            long receiveTime = System.nanoTime();

            // Verify sequence
            int responseSeq = buffer.getInt(0);
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

        long totalBytes = (long) messageCount * messageSize * 2;  // request + response
        double bytesPerSec = totalBytes / (durationMs / 1000.0);
        double mbPerSec = bytesPerSec / (1024 * 1024);

        System.out.println();
        System.out.println("=== Socket Ping-Pong Latency (round-trip) ===");
        System.out.printf("  Min:    %,d ns (%.2f µs)%n", minLatency, minLatency / 1000.0);
        System.out.printf("  Avg:    %,.0f ns (%.2f µs)%n", avgLatency, avgLatency / 1000.0);
        System.out.printf("  Max:    %,d ns (%.2f µs)%n", maxLatency, maxLatency / 1000.0);
        System.out.println();
        System.out.printf("  Round-trips:  %,d%n", messageCount);
        System.out.printf("  Duration:     %.2f ms%n", durationMs);
        System.out.printf("  Throughput:   %,.0f rt/sec%n", throughput);
        System.out.printf("  Bandwidth:    %,.2f MB/sec%n", mbPerSec);
    }

    private static void readFully(SocketChannel channel, ByteBuffer buffer) throws Exception {
        while (buffer.hasRemaining()) {
            if (channel.read(buffer) < 0) {
                throw new RuntimeException("Unexpected end of stream");
            }
        }
    }

    private static void writeFully(SocketChannel channel, ByteBuffer buffer) throws Exception {
        while (buffer.hasRemaining()) {
            channel.write(buffer);
        }
    }
}
