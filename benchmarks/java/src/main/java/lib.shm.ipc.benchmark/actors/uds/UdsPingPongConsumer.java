package lib.shm.ipc.benchmark.actors.uds;

import lib.shm.ipc.benchmark.actors.ActorConfig;

import java.net.StandardProtocolFamily;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.file.Files;

public final class UdsPingPongConsumer extends UdsBenchmarkActor {
    private ServerSocketChannel server;

    public UdsPingPongConsumer(String pathSuffix) {
        super(pathSuffix);
    }

    @Override
    protected void onInit(ActorConfig config) throws Exception {
        super.onInit(config);
        server = ServerSocketChannel.open(StandardProtocolFamily.UNIX);

        Files.deleteIfExists(UNIX_DOMAIN_SOCKET_ADDRESS.getPath());
        server.bind(UNIX_DOMAIN_SOCKET_ADDRESS);
    }

    @Override
    protected void onHandshake(ActorConfig config) throws Exception {
        channel = server.accept();
    }

    @Override
    protected void onStop(ActorConfig config) throws Exception {
        channel.close();
        server.close();
    }

    @Override
    protected void pingPong(SocketChannel client) throws Exception {
        readFully(client);
        writeFully(client);
    }
}
