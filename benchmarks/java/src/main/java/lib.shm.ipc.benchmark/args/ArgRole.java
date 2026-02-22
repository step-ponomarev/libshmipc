package lib.shm.ipc.benchmark.args;

public enum ArgRole {
    SHM_PING_PONG_PRODUCER("shm-ping-pong-producer"),
    SHM_PING_PONG_CONSUMER("shm-ping-pong-consumer"),
    TCP_PING_PONG_PRODUCER("tcp-ping-pong-producer"),
    TCP_PING_PONG_CONSUMER("tcp-ping-pong-consumer"),
    UDS_PING_PONG_PRODUCER("uds-ping-pong-producer"),
    UDS_PING_PONG_CONSUMER("uds-ping-pong-consumer");
    public final String role;

    ArgRole(String role) {
        this.role = role;
    }

    public static ArgRole of(String role) {
        for (ArgRole r : ArgRole.values()) {
            if (r.role.equals(role)) {
                return r;
            }
        }

        throw new IllegalArgumentException("Unknown role: " + role);
    }
}
