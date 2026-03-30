package lib.shm.ipc.benchmark.actors;

import lib.shm.ipc.benchmark.args.ArgRole;

public enum Mode {
    SHM_PING_PONG("shm-ping-pong", ArgRole.SHM_PING_PONG_PRODUCER, ArgRole.SHM_PING_PONG_CONSUMER),
    UDS_PING_PONG("uds-ping-pong", ArgRole.UDS_PING_PONG_PRODUCER, ArgRole.UDS_PING_PONG_CONSUMER);

    public final String mode;
    public final String producerRole;
    public final String consumerRole;

    Mode(String mode, ArgRole producerRole, ArgRole consumerRole) {
        this.mode = mode;
        this.producerRole = producerRole.role;
        this.consumerRole = consumerRole.role;
    }
}
