package lib.shm.ipc.channel;

import lib.shm.ipc.IpcStatus;
import lib.shm.ipc.LibLoader;
import lib.shm.ipc.exeption.IpcException;
import lib.shm.ipc.result.IpcResultWrapper;
import org.junit.Assert;
import org.junit.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class IpcChannelTest {
    @Test
    public void basicSingleThreadTest() throws IpcException {
        final long size = IpcChannel.getSuggestedSize(2000);
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment memory = arena.allocate(size);
            IpcChannel producer = IpcChannel.create(arena, memory, size).getResult();
            IpcChannel consumer = IpcChannel.connect(arena, memory).getResult();

            String testMsg = "Hello";
            byte[] bytes = testMsg.getBytes(StandardCharsets.UTF_8);

            IpcResultWrapper<Void> writeResult = producer.write(bytes);
            Assert.assertEquals(IpcStatus.IPC_OK, writeResult.getStatus());

            IpcResultWrapper<byte[]> readResult = consumer.read(200);
            Assert.assertEquals(IpcStatus.IPC_OK, readResult.getStatus());

            Assert.assertEquals(testMsg, new String(readResult.getResult(), StandardCharsets.UTF_8));
        }
    }

    @Test
    public void basicProducerConsumerTest() throws InterruptedException, IpcException {
        LibLoader.load();

        final int count = 1_000_000;
        final long size = IpcChannel.getSuggestedSize(200);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final AtomicInteger received = new AtomicInteger(0);
            final MemorySegment memory = arena.allocate(size);
            IpcChannel producer = IpcChannel.create(arena, memory, size).getResult();
            IpcChannel consumer = IpcChannel.connect(arena, memory).getResult();

            final String messageTemplate = "Message %d";
            exec.execute(() -> {
                for (int i = 0; i < count; i++) {
                    String formatted = messageTemplate.formatted(i);
                    byte[] bytes = formatted.getBytes(StandardCharsets.UTF_8);
                    try {
                        producer.write(bytes);
                    } catch (IpcException e) {
                        i--;
                    }

                }
            });

            exec.execute(() -> {
                while (true) {
                    final IpcResultWrapper<byte[]> readResult;
                    try {
                        readResult = consumer.read(TimeUnit.SECONDS.toMillis(1));
                        if (readResult.getStatus() == IpcStatus.IPC_OK) {
                            final String expectedMessage = messageTemplate.formatted(received.getAndIncrement());
                            String message = new String(readResult.getResult(), StandardCharsets.UTF_8);
                            Assert.assertEquals(expectedMessage, message);
                        }
                    } catch (IpcException e) {
                    }

                    if (received.get() == count) {
                        return;
                    }
                }
            });

            exec.shutdown();
            exec.awaitTermination(10, TimeUnit.SECONDS);
        }
    }

    @Test(timeout = 1500)
    public void timeout() throws IpcException {
        final long readTimeoutMs = 1000;
        final long size = IpcChannel.getSuggestedSize(2000);
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment memory = arena.allocate(size);
            IpcChannel.create(arena, memory, size);
            IpcChannel consumer = IpcChannel.connect(arena, memory).getResult();

            long beforeRead = System.currentTimeMillis();
            consumer.read(readTimeoutMs);
            Assert.assertTrue(System.currentTimeMillis() - beforeRead >= readTimeoutMs);
        }
    }
}
