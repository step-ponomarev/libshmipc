package lib.shm.ipc.benchmark;

import java.net.InetSocketAddress;
import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.SocketChannel;
import java.nio.file.Path;
import java.util.Random;

/**
 * Socket ping-pong client for accurate latency measurement.
 * Sends a message and waits for response before sending next.
 *
 * Usage: java SocketPingPongClient <mode> <host:port_or_path> <message_count> <warmup_count> <message_size_or_sizes>
 */
public class SocketPingPongClient {

    public static void main(String[] args) throws Exception {
        if (args.length < 5) {
            System.err.println("Usage: SocketPingPongClient <tcp|unix> <host:port_or_path> <message_count> <warmup_count> <message_size_or_sizes>");
            System.exit(1);
        }

        String mode = args[0];
        String target = args[1];
        int messageCount = Integer.parseInt(args[2]);
        int warmupCount = Integer.parseInt(args[3]);
        int[] messageSizes = BenchmarkUtils.parseSizes(args[4]);

        if ("tcp".equals(mode)) {
            String[] parts = target.split(":");
            String host = parts[0];
            int port = Integer.parseInt(parts[1]);
            runTcpClient(host, port, messageCount, warmupCount, messageSizes);
        } else if ("unix".equals(mode)) {
            runUnixClient(Path.of(target), messageCount, warmupCount, messageSizes);
        } else {
            System.err.println("Unknown mode: " + mode);
            System.exit(1);
        }
    }

    private static void runTcpClient(String host, int port, int messageCount, int warmupCount, int[] messageSizes) throws Exception {
        System.out.printf("TCP PingPong Client connecting to %s:%d%n", host, port);

        try (SocketChannel channel = SocketChannel.open()) {
            channel.connect(new InetSocketAddress(host, port));
            channel.socket().setTcpNoDelay(true);
            runPingPong(channel, messageCount, warmupCount, messageSizes);
        }
    }

    private static void runUnixClient(Path socketPath, int messageCount, int warmupCount, int[] messageSizes) throws Exception {
        System.out.printf("Unix PingPong Client connecting to %s%n", socketPath);

        try (SocketChannel channel = SocketChannel.open(StandardProtocolFamily.UNIX)) {
            channel.connect(UnixDomainSocketAddress.of(socketPath));
            runPingPong(channel, messageCount, warmupCount, messageSizes);
        }
    }

    private static void runPingPong(SocketChannel channel, int messageCount, int warmupCount, int[] messageSizes) throws Exception {
        int maxMessageSize = BenchmarkUtils.maxSize(messageSizes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(maxMessageSize).order(ByteOrder.nativeOrder());
        ByteBuffer header = ByteBuffer.allocateDirect(Integer.BYTES).order(ByteOrder.nativeOrder());
        Random random = new Random();

        System.out.println("Warmup phase: " + warmupCount + " round-trips");
        for (int i = 0; i < warmupCount; i++) {
            int messageSize = messageSizes[random.nextInt(messageSizes.length)];
            buffer.clear();
            buffer.putLong(System.nanoTime());
            buffer.position(messageSize);
            buffer.flip();
            BenchmarkUtils.writeFrame(channel, header, buffer, messageSize);

            BenchmarkUtils.readFrame(channel, header, buffer, maxMessageSize);
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

            buffer.clear();
            buffer.putLong(sendTime);
            buffer.putInt(i);
            buffer.position(messageSize);
            buffer.flip();
            BenchmarkUtils.writeFrame(channel, header, buffer, messageSize);

            int responseSize = BenchmarkUtils.readFrame(channel, header, buffer, maxMessageSize);
            buffer.limit(responseSize);

            long receiveTime = System.nanoTime();

            int responseSeq = buffer.getInt(0);
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

        double bytesPerSec = totalBytes / (durationMs / 1000.0);
        double mbPerSec = bytesPerSec / (1024 * 1024);

        System.out.println();
        System.out.println("=== Socket Ping-Pong Latency (round-trip) ===");
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
    }

}
