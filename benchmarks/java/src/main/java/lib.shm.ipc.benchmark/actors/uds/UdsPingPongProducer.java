package lib.shm.ipc.benchmark.actors.uds;

import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.utils.HistogramUtils;
import org.HdrHistogram.Histogram;

import java.net.StandardProtocolFamily;
import java.nio.channels.SocketChannel;

public final class UdsPingPongProducer extends UdsBenchmarkActor {
    private Histogram hist;

    public UdsPingPongProducer(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {
        super.onInit(config);
        channel = SocketChannel.open(StandardProtocolFamily.UNIX);
        hist = new Histogram(1, 60_000_000_000L, 3);
    }

    @Override
    protected void onHandshake(ActorConfig config) throws Exception {
        channel.connect(UNIX_DOMAIN_SOCKET_ADDRESS);
    }

    @Override
    protected byte[] onResult(ActorConfig config) throws Exception {
        return HistogramUtils.toLatencyResult(hist).serialize();
    }

    @Override
    protected void onStop(ActorConfig config) throws Exception {
        channel.close();
    }

    @Override
    protected void pingPong(SocketChannel client) throws Exception {
        final long t0 = System.nanoTime();
        writeFully(client);
        readFully(client);
        long tn = System.nanoTime() - t0;
        hist.recordValue(tn);
    }
}
