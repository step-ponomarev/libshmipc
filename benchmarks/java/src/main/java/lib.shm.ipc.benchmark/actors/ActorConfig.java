package lib.shm.ipc.benchmark.actors;

public record ActorConfig(
        int messageCount,
        int warmupCount,
        int messageSize,
        int bufferSize
) {}
