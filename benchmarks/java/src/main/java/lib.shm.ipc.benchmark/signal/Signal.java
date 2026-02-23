package lib.shm.ipc.benchmark.signal;

public enum Signal {
    INIT((byte) 1),
    HANDSHAKE((byte) 2),
    WARMUP((byte) 3),
    MEASURE((byte) 4),
    RESULT((byte) 5),
    STOP((byte) 6),
    DONE((byte) 7);
    private final byte value;

    Signal(byte value) {
        this.value = value;
    }

    public byte[] bytes() {
        return new byte[]{value};
    }

    public static Signal valueOf(byte[] bytes) {
        if (bytes.length != 1) {
            throw new IllegalArgumentException("Invalid byte array length: " + bytes.length);
        }

        return valueOf(bytes[0]);
    }

    public static Signal valueOf(byte value) {
        for (Signal signal : values()) {
            if (signal.value == value) {
                return signal;
            }
        }

        throw new IllegalArgumentException();
    }
}
