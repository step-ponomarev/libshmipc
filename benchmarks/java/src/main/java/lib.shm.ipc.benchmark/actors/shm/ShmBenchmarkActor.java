package lib.shm.ipc.benchmark.actors.shm;

import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.BenchmarkActor;
import lib.shm.ipc.channel.IpcChannel;

public abstract class ShmBenchmarkActor extends BenchmarkActor {
    protected final static String DATA_BUFFER_PREFIX = "/tmp/ipc_data_benchmark";

    protected SharedMemoryFile inShm;
    protected SharedMemoryFile outShm;

    protected IpcChannel inChannel;
    protected IpcChannel outChannel;

    protected ShmBenchmarkActor(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected final void onStop(ActorConfig config) throws Exception {
        if (inChannel != null) {
            inChannel.close();
            inChannel = null;
        }

        if (outChannel != null) {
            outChannel.close();
            outChannel = null;
        }

        if (inShm != null) {
            inShm.close();
            inShm = null;
        }

        if (outShm != null) {
            outShm.close();
            outShm = null;
        }
    }
}
