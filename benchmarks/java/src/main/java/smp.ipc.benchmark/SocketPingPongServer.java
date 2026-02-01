package lib.shm.ipc.benchmark;

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
 * Usage: java SocketPingPongServer <mode> <port_or_path> <message_count> <warmup_count> <message_size>
 */
public class SocketPingPongServer {

    public static void main(String[] args) throws Exception {
        if (args.length < 5) {
            System.err.println("Usage: SocketPingPongServer <tcp|unix> <port_or_path> <message_count> <warmup_count> <message_size>");
            System.exit(1);
        }

        String mode = args[0];
        String portOrPath = args[1];
        int messageCount = Integer.parseInt(args[2]);
        int warmupCount = Integer.parseInt(args[3]);
        int messageSize = Integer.parseInt(args[4]);

        if ("tcp".equals(mode)) {
            runTcpServer(Integer.parseInt(portOrPath), messageCount, warmupCount, messageSize);
        } else if ("unix".equals(mode)) {
            runUnixServer(Path.of(portOrPath), messageCount, warmupCount, messageSize);
        } else {
            System.err.println("Unknown mode: " + mode);
            System.exit(1);
        }
    }

    private static void runTcpServer(int port, int messageCount, int warmupCount, int messageSize) throws Exception {
        System.out.printf("TCP PingPong Server starting on port %d%n", port);

        try (ServerSocketChannel serverChannel = ServerSocketChannel.open()) {
            serverChannel.bind(new java.net.InetSocketAddress(port));
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

    private static void runEchoLoop(SocketChannel channel, int messageCount, int warmupCount, int messageSize) throws Exception {
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(messageSize).order(ByteOrder.nativeOrder());
        ByteBuffer writeBuffer = ByteBuffer.allocateDirect(messageSize).order(ByteOrder.nativeOrder());

        int totalMessages = warmupCount + messageCount;
        System.out.printf("Echo loop: %d messages (warmup=%d, measure=%d)%n", totalMessages, warmupCount, messageCount);

        for (int i = 0; i < totalMessages; i++) {
            // Read request
            readBuffer.clear();
            readFully(channel, readBuffer);
            readBuffer.flip();

            // Extract sequence from request (at offset 8, after timestamp)
            int seq = readBuffer.getInt(8);

            // Send response with sequence at offset 0
            writeBuffer.clear();
            writeBuffer.putInt(seq);
            writeBuffer.position(messageSize);
            writeBuffer.flip();
            writeFully(channel, writeBuffer);
        }

        System.out.println("Server done.");
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
