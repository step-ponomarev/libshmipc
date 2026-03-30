package lib.shm.ipc.benchmark;

import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.shm.ShmPingPongProducer;
import lib.shm.ipc.benchmark.actors.shm.ShmPingPongConsumer;
import lib.shm.ipc.benchmark.actors.uds.UdsPingPongConsumer;
import lib.shm.ipc.benchmark.actors.uds.UdsPingPongProducer;
import lib.shm.ipc.benchmark.args.ArgsUtils;
import lib.shm.ipc.benchmark.args.ArgRole;

import java.util.Map;

import static lib.shm.ipc.benchmark.args.ArgsUtils.ARG_BUFFER_SIZE;

public final class Runner {
    private static final String[] REQUIRED_ARGS = {
            ArgsUtils.ARG_ROLE,
            ArgsUtils.ARG_MESSAGE_COUNT,
            ArgsUtils.ARG_WARMUP_COUNT,
            ArgsUtils.ARG_MESSAGE_SIZE
    };

    static void main(String[] args) throws Exception {
        final Map<String, String> params = ArgsUtils.getArgs(args);
        for (String arg : REQUIRED_ARGS) {
            if (!params.containsKey(arg)) {
                throw new IllegalArgumentException(String.format("Missing required argument '%s'", arg));
            }
        }

        final ActorConfig actorConfig = new ActorConfig(
                Integer.parseInt(params.get(ArgsUtils.ARG_MESSAGE_COUNT)),
                Integer.parseInt(params.get(ArgsUtils.ARG_WARMUP_COUNT)),
                Integer.parseInt(params.get(ArgsUtils.ARG_MESSAGE_SIZE)),
                Integer.parseInt(params.get(ARG_BUFFER_SIZE))
        );

        final ArgRole role = ArgRole.of(params.get(ArgsUtils.ARG_ROLE));
        switch (role) {
            case ArgRole.SHM_PING_PONG_PRODUCER ->
                    new ShmPingPongProducer(ArgRole.SHM_PING_PONG_PRODUCER.role).run(actorConfig);
            case ArgRole.SHM_PING_PONG_CONSUMER ->
                    new ShmPingPongConsumer(ArgRole.SHM_PING_PONG_CONSUMER.role).run(actorConfig);
            case ArgRole.UDS_PING_PONG_PRODUCER ->
                    new UdsPingPongProducer(ArgRole.UDS_PING_PONG_PRODUCER.role).run(actorConfig);
            case ArgRole.UDS_PING_PONG_CONSUMER ->
                    new UdsPingPongConsumer(ArgRole.UDS_PING_PONG_CONSUMER.role).run(actorConfig);

            default -> throw new UnsupportedOperationException("Unsupported role: " + role);
        }
    }
}
