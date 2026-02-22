package lib.shm.ipc.benchmark.actors.uds;

import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.BenchmarkActor;

public final class UdsPingPongConsumer extends BenchmarkActor {
    public UdsPingPongConsumer(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {}

    @Override
    protected void onStop(ActorConfig config) throws Exception {}
}
