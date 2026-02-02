package lib.shm.ipc.benchmark;

import java.net.InetSocketAddress;
import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Socket ping-pong server (echo).
 * Receives a message and immediately sends response.
 *
 * Usage: java SocketPingPongServer <mode> <port_or_path> <message_count> <warmup_count> <message_size_or_sizes>
 */
public class SocketPingPongServer {

    public static void main(String[] args) throws Exception {
        if (args.length < 5) {
            System.err.println("Usage: SocketPingPongServer <tcp|unix> <port_or_path> <message_count> <warmup_count> <message_size_or_sizes>");
            System.exit(1);
        }

        String mode = args[0];
        String portOrPath = args[1];
        int messageCount = Integer.parseInt(args[2]);
        int warmupCount = Integer.parseInt(args[3]);
        int[] messageSizes = BenchmarkUtils.parseSizes(args[4]);
        int maxMessageSize = BenchmarkUtils.maxSize(messageSizes);

        if ("tcp".equals(mode)) {
            runTcpServer(Integer.parseInt(portOrPath), messageCount, warmupCount, maxMessageSize);
        } else if ("unix".equals(mode)) {
            runUnixServer(Path.of(portOrPath), messageCount, warmupCount, maxMessageSize);
        } else {
            System.err.println("Unknown mode: " + mode);
            System.exit(1);
        }
    }

    private static void runTcpServer(int port, int messageCount, int warmupCount, int messageSize) throws Exception {
        System.out.printf("TCP PingPong Server starting on port %d%n", port);

        try (ServerSocketChannel serverChannel = ServerSocketChannel.open()) {
            serverChannel.bind(new InetSocketAddress(port));
            System.out.println("Waiting for client...");
            try (SocketChannel channel = serverChannel.accept()) {
                channel.socket().setTcpNoDelay(true);
                runEchoLoop(channel, messageCount, warmupCount, messageSize);
            }
        }
    }

    private static void runUnixServer(Path socketPath, int messageCount, int warmupCount, int messageSize) throws Exception {
        System.out.printf("Unix PingPong Server starting at %s%n", socketPath);

        Files.deleteIfExists(socketPath);

        try (ServerSocketChannel serverChannel = ServerSocketChannel.open(StandardProtocolFamily.UNIX)) {
            serverChannel.bind(UnixDomainSocketAddress.of(socketPath));
            System.out.println("Waiting for client...");
            try (SocketChannel channel = serverChannel.accept()) {
                runEchoLoop(channel, messageCount, warmupCount, messageSize);
            }
        } finally {
            Files.deleteIfExists(socketPath);
        }
    }

    private static void runEchoLoop(SocketChannel channel, int messageCount, int warmupCount, int maxMessageSize) throws Exception {
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(maxMessageSize).order(ByteOrder.nativeOrder());
        ByteBuffer writeBuffer = ByteBuffer.allocateDirect(maxMessageSize).order(ByteOrder.nativeOrder());
        ByteBuffer header = ByteBuffer.allocateDirect(Integer.BYTES).order(ByteOrder.nativeOrder());

        int totalMessages = warmupCount + messageCount;
        System.out.printf("Echo loop: %d messages (warmup=%d, measure=%d)%n", totalMessages, warmupCount, messageCount);

        for (int i = 0; i < totalMessages; i++) {
            int messageSize = BenchmarkUtils.readFrame(channel, header, readBuffer, maxMessageSize);
            int seq = readBuffer.getInt(8);

            writeBuffer.clear();
            writeBuffer.putInt(seq);
            writeBuffer.position(messageSize);
            writeBuffer.flip();
            BenchmarkUtils.writeFrame(channel, header, writeBuffer, messageSize);
        }

        System.out.println("Server done.");
    }

}
