package lib.shm.ipc.benchmark;

public enum Role {
    SHM_PRODUCER("shm-producer"),
    SHM_CONSUMER("shm-consumer"),
    TCP_PRODUCER("tcp-producer"),
    TCP_CONSUMER("tcp-consumer"),
    UDS_PRODUCER("uds-producer"),
    UDS_CONSUMER("uds-consumer");
    public final String role;

    Role(String role) {
        this.role = role;
    }

    public static Role of(String role) {
        for (Role r : Role.values()) {
            if (r.role.equals(role)) {
                return r;
            }
        }

        throw new IllegalArgumentException("Unknown role: " + role);
    }
}
