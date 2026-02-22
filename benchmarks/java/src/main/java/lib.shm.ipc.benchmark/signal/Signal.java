package lib.shm.ipc.benchmark.signal;

public enum Signal {
    INIT((byte) 1),
    WARMUP((byte) 2),
    MEASURE((byte) 3),
    RESULT((byte) 4),
    STOP((byte) 5),
    DONE((byte) 6);
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
