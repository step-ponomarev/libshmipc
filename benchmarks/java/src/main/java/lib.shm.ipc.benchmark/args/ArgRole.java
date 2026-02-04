package lib.shm.ipc.benchmark.args;

public enum ArgRole {
    SHM_PING_PONG_PRODUCER("shm-ping-pong-producer"),
    SHM_PING_PONG_CONSUMER("shm-ping-pong-consumer"),
    TCP_PRODUCER("tcp-producer"),
    TCP_CONSUMER("tcp-consumer"),
    UDS_PRODUCER("uds-producer"),
    UDS_CONSUMER("uds-consumer");
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
