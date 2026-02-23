package lib.shm.ipc.benchmark.actors;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public record LatencyResult(
        double p50Us,
        double p95Us,
        double p99Us,
        double p99_99Us,
        double minUs,
        double maxUs
) {
    public static final int BYTES = Double.BYTES * 6;

    public byte[] serialize() {
        ByteBuffer buf = ByteBuffer.allocate(BYTES).order(ByteOrder.LITTLE_ENDIAN);
        buf.putDouble(p50Us);
        buf.putDouble(p95Us);
        buf.putDouble(p99Us);
        buf.putDouble(p99_99Us);
        buf.putDouble(minUs);
        buf.putDouble(maxUs);
        return buf.array();
    }

    public static LatencyResult deserialize(byte[] data) {
        if (data == null || data.length != BYTES) {
            throw new IllegalArgumentException("Invalid data length: " + (data == null ? "null" : data.length)
                    + ", expected at least " + BYTES);
        }

        ByteBuffer buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        double p50 = buf.getDouble();
        double p95 = buf.getDouble();
        double p99 = buf.getDouble();
        double p9999 = buf.getDouble();
        double min = buf.getDouble();
        double max = buf.getDouble();

        return new LatencyResult(p50, p95, p99, p9999, min, max);
    }
}
