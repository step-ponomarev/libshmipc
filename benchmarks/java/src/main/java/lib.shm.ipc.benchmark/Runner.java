package lib.shm.ipc.benchmark;

import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.ShmPingPongProducer;
import lib.shm.ipc.benchmark.actors.ShmPingPongConsumer;
import java.util.Map;

public final class Runner {

    static void main(String[] args) throws Exception {
        final Map<String, String> params = ArgsUtils.getArgs(args);
        final String role = params.get(ArgsUtils.ARG_ROLE);
        if (role == null) {
            throw new IllegalArgumentException("Missing required argument " + ArgsUtils.ARG_ROLE);
        }

        final ActorConfig actorConfig = new ActorConfig(
                Integer.parseInt(params.getOrDefault(ArgsUtils.ARG_MESSAGE_COUNT, "1000000")),
                Integer.parseInt(params.getOrDefault(ArgsUtils.ARG_WARMUP_COUNT, "10000")),
                Integer.parseInt(params.getOrDefault(ArgsUtils.ARG_MESSAGE_SIZE, "64000")),
                Integer.parseInt(params.getOrDefault(ArgsUtils.ARG_BUFFER_SIZE, 64000 * 16 + ""))
        );

        switch (Role.of(role)) {
            case SHM_PRODUCER:
                new ShmPingPongProducer().run(actorConfig);
                break;
            case SHM_CONSUMER:
                new ShmPingPongConsumer().run(actorConfig);
                break;
            default:
                throw new UnsupportedOperationException("Unsupported role: " + role);
        }
    }
}
