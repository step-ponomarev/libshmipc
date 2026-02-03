package lib.shm.ipc.benchmark;

import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.util.Arrays;

final class BenchmarkUtils {
    static final int MIN_MESSAGE_SIZE = 12;

    private BenchmarkUtils() {
    }

    static int[] parseSizes(String arg) {
        String[] parts = arg.split("[,\\s]+");
        int[] sizes = new int[parts.length];
        int count = 0;
        for (String part : parts) {
            if (part.isBlank()) {
                continue;
            }

            int size = Integer.parseInt(part.trim());
            if (size < MIN_MESSAGE_SIZE) {
                size = MIN_MESSAGE_SIZE;
            }

            sizes[count++] = size;
        }

        if (count == 0) {
            throw new IllegalArgumentException("No message sizes provided");
        }

        if (count == sizes.length) {
            return sizes;
        }

        int[] result = new int[count];
        System.arraycopy(sizes, 0, result, 0, count);
        return result;
    }

    static int maxSize(int[] sizes) {
        int max = sizes[0];
        for (int size : sizes) {
            if (size > max) {
                max = size;
            }
        }
        return max;
    }

    static String sizesToString(int[] sizes) {
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < sizes.length; i++) {
            if (i > 0) {
                builder.append(",");
            }

            builder.append(sizes[i]);
        }
        return builder.toString();
    }

    static double percentile(long[] values, int percentile) {
        long[] copy = Arrays.copyOf(values, values.length);
        Arrays.sort(copy);

        int index = (int) Math.ceil(percentile / 100.0 * copy.length) - 1;
        if (index < 0) {
            index = 0;
        } else if (index >= copy.length) {
            index = copy.length - 1;
        }

        return copy[index];
    }

    static void readFully(SocketChannel channel, ByteBuffer buffer) throws Exception {
        while (buffer.hasRemaining()) {
            if (channel.read(buffer) < 0) {
                throw new RuntimeException("Unexpected end of stream");
            }
        }
    }

    static void writeFully(SocketChannel channel, ByteBuffer buffer) throws Exception {
        while (buffer.hasRemaining()) {
            channel.write(buffer);
        }
    }

    static void writeFrame(SocketChannel channel, ByteBuffer header, ByteBuffer payload, int size) throws Exception {
        if (size < MIN_MESSAGE_SIZE) {
            throw new RuntimeException("Invalid message size: " + size);
        }

        header.clear();
        header.putInt(size);
        header.flip();
        writeFully(channel, header);
        payload.limit(size);
        writeFully(channel, payload);
    }

    static int readFrame(SocketChannel channel, ByteBuffer header, ByteBuffer payload, int maxMessageSize) throws Exception {
        header.clear();
        readFully(channel, header);
        header.flip();
        int size = header.getInt();
        if (size < MIN_MESSAGE_SIZE || size > maxMessageSize) {
            throw new RuntimeException("Invalid message size: " + size);
        }
        payload.clear();
        payload.limit(size);
        readFully(channel, payload);
        payload.flip();
        return size;
    }
}
