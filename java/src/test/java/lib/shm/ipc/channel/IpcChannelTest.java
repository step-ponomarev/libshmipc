package lib.shm.ipc.channel;

import lib.shm.ipc.IpcStatus;
import lib.shm.ipc.LibLoader;
import lib.shm.ipc.jextract.channel.*;
import org.junit.Assert;
import org.junit.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import lib.shm.ipc.result.*;


public class IpcChannelTest {
    @Test
    public void basicSingleThreadTest() {
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
    public void basicProducerConsumerTest() throws InterruptedException {
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
                    IpcResultWrapper<Void> write = producer.write(bytes);
                    if (IpcStatus.IPC_OK != write.getStatus()) {
                        i--;
                    }
                }
            });

            final MemorySegment timeout = timespec.allocate(arena);
            timespec.tv_sec(timeout, 1);
            timespec.tv_nsec(timeout, 0L);
            exec.execute(() -> {
                while (true) {
                    final IpcResultWrapper<byte[]> readResult = consumer.read(TimeUnit.SECONDS.toMillis(200));
                    if (readResult.getStatus() != IpcStatus.IPC_OK) {
                        if (received.get() == count) {
                            return;
                        }

                        continue;
                    }

                    final String expectedMessage = messageTemplate.formatted(received.getAndIncrement());
                    String message = new String(readResult.getResult(), StandardCharsets.UTF_8);
                    Assert.assertEquals(expectedMessage, message);
                }
            });

            exec.shutdown();
            exec.awaitTermination(10, TimeUnit.SECONDS);
        }
    }
}
