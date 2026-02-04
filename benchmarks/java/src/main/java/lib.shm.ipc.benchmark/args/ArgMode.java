package lib.shm.ipc.benchmark.args;

public enum ArgMode {
    SHM_LATENCY("shm-latency", ArgRole.SHM_PING_PONG_PRODUCER, ArgRole.SHM_PING_PONG_CONSUMER);
    public final String mode;
    public final String producerRole;
    public final String consumerRole;

    ArgMode(String mode, ArgRole producerRole, ArgRole consumerRole) {
        this.mode = mode;
        this.producerRole = producerRole.role;
        this.consumerRole = consumerRole.role;
    }

    public static ArgMode of(String role) {
        for (ArgMode r : ArgMode.values()) {
            if (r.mode.equals(role)) {
                return r;
            }
        }

        throw new IllegalArgumentException("Unknown role: " + role);
    }
}
