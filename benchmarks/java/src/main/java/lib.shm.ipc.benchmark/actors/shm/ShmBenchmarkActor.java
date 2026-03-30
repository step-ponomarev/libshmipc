package lib.shm.ipc.benchmark.actors.shm;

import ipc.channel.IpcChannel;
import lib.shm.ipc.benchmark.SharedMemoryFile;
import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.BenchmarkActor;

public abstract class ShmBenchmarkActor extends BenchmarkActor {
    protected final static String DATA_BUFFER_PATH = "/tmp/ipc_shm_data_benchmark";

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
