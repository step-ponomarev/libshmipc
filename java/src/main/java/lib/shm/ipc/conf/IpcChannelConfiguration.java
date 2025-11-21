package lib.shm.ipc.conf;

public final class IpcChannelConfiguration {
    public final int maxRoundTrips;
    public final long startSeepNs;
    public final long maxSleepNs;
    public final boolean create;

    public IpcChannelConfiguration(int maxRoundTrips, long startSeepNs, long maxSleepNs, boolean create) {
        this.maxRoundTrips = maxRoundTrips;
        this.startSeepNs = startSeepNs;
        this.maxSleepNs = maxSleepNs;
        this.create = create;
    }
}
