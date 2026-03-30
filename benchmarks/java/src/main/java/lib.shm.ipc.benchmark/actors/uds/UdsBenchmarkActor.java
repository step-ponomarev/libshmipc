package lib.shm.ipc.benchmark.actors.uds;

import lib.shm.ipc.benchmark.actors.ActorConfig;
import lib.shm.ipc.benchmark.actors.BenchmarkActor;

import java.io.IOException;
import java.net.UnixDomainSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;

public abstract class UdsBenchmarkActor extends BenchmarkActor {
    protected SocketChannel channel;

    protected final static UnixDomainSocketAddress UNIX_DOMAIN_SOCKET_ADDRESS = UnixDomainSocketAddress.of("/tmp/ipc_uds_data_benchmark");
    private byte[] bytes;
    private ByteBuffer message;

    protected UdsBenchmarkActor(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {
        bytes = new byte[config.messageSize()];
        message = ByteBuffer.wrap(bytes);
    }

    @Override
    protected void onWarmup(ActorConfig config) throws Exception {
        for (int i = 0; i < config.warmupCount(); i++) {
            pingPong(channel);
        }
    }

    @Override
    protected void onMeasure(ActorConfig config) throws Exception {
        for (int i = 0; i < config.messageCount(); i++) {
            pingPong(channel);
        }
    }

    protected abstract void pingPong(SocketChannel client) throws Exception;

    protected void writeFully(SocketChannel ch) throws IOException {
        message.rewind();
        while (message.hasRemaining()) {
            ch.write(message);
        }
    }

    protected void readFully(SocketChannel ch) throws IOException {
        message.clear();
        while (message.hasRemaining()) {
            int r = ch.read(message);
            if (r == -1) {
                throw new IOException("Peer closed while reading");
            }
        }
    }
}
