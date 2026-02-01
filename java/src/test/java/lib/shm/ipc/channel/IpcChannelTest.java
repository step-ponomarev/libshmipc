package lib.shm.ipc.channel;

import lib.shm.ipc.LibLoader;
import lib.shm.ipc.exeption.IpcException;
import lib.shm.ipc.exeption.IpcTimeoutException;
import org.junit.Assert;
import org.junit.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class IpcChannelTest {
    private static final Object STUB = new Object();

    @Test
    public void basicSingleThreadTest() throws IpcException {
        final long size = IpcChannel.getSuggestedSize(2000);
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment memory = arena.allocate(size);
            IpcChannel producer = IpcChannel.create(memory, size);
            IpcChannel consumer = IpcChannel.connect(memory);

            String testMsg = "Hello";
            byte[] bytes = testMsg.getBytes(StandardCharsets.UTF_8);

            producer.write(bytes);

            byte[] readResult = consumer.read(Duration.ofMillis(200));
            Assert.assertEquals(testMsg, new String(readResult, StandardCharsets.UTF_8));
        }
    }

    @Test(timeout = 30000)
    public void basicProducerConsumerTest() throws InterruptedException, IpcException {
        LibLoader.load();

        final int count = 1_000_000;
        final long size = IpcChannel.getSuggestedSize(200);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final MemorySegment memory = arena.allocate(size);
            IpcChannel producer = IpcChannel.create(memory, size);
            IpcChannel consumer = IpcChannel.connect(memory);

            final String messageTemplate = "Message %d";
            final ConcurrentHashMap<String, Object> sendEntities = new ConcurrentHashMap<>();
            exec.execute(() -> {
                for (int i = 0; i < count; i++) {
                    String formatted = messageTemplate.formatted(i);
                    byte[] bytes = formatted.getBytes(StandardCharsets.UTF_8);
                    try {
                        producer.write(bytes);
                        sendEntities.put(formatted, STUB);
                    } catch (IpcException e) {
                        i--;
                    }
                }
            });

            final ConcurrentHashMap<String, Object> receivedEntities = new ConcurrentHashMap<>();
            exec.execute(() -> {
                while (true) {
                    final byte[] readResult;
                    try {
                        readResult = consumer.read(Duration.ofSeconds(1));
                        String message = new String(readResult, StandardCharsets.UTF_8);
                        receivedEntities.put(message, STUB);
                    } catch (IpcException e) {}

                    if (receivedEntities.size() == count) {
                        return;
                    }
                }
            });

            exec.shutdown();
            exec.awaitTermination(10, TimeUnit.SECONDS);
            Assert.assertEquals(sendEntities.keySet(), receivedEntities.keySet());
            Assert.assertEquals(count, sendEntities.size());
        }
    }

    @Test(timeout = 60000)
    public void basicMultiProducerMultiConsumerTest() throws InterruptedException, IpcException {
        LibLoader.load();

        final int count = 1_000_000;
        final long size = IpcChannel.getSuggestedSize(200);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final MemorySegment memory = arena.allocate(size);
            final AtomicInteger send = new AtomicInteger(0);
            final ConcurrentHashMap<String, Object> sendEntities = new ConcurrentHashMap<>();
            final String messageTemplate = "Message %d";

            IpcChannel.create(memory, size); // initialize
            for (int i = 0; i < 2; i++) {
                IpcChannel producer = IpcChannel.connect( memory);
                exec.execute(() -> {
                    while (true) {
                        final int num = send.getAndIncrement();
                        if (num >= count) {
                            break;
                        }

                        final String msg = messageTemplate.formatted(num);
                        while (true) {
                            try {
                                producer.write(msg.getBytes(StandardCharsets.UTF_8));
                                sendEntities.put(msg, STUB);
                                break;
                            } catch (Exception e) {}
                        }
                    }
                });
            }

            final ConcurrentHashMap<String, Object> receivedEntities = new ConcurrentHashMap<>();
            for (int i = 0; i < 10; i++) {
                IpcChannel consumer = IpcChannel.connect(memory);
                exec.execute(() -> {
                    while (receivedEntities.size() != count) {
                        final byte[] readResult;
                        try {
                            readResult = consumer.read(Duration.ofSeconds(1));
                            String message = new String(readResult, StandardCharsets.UTF_8);
                            receivedEntities.put(message, STUB);
                        } catch (IpcException e) {}
                    }
                });
            }

            exec.shutdown();
            exec.awaitTermination(10, TimeUnit.SECONDS);
            Assert.assertEquals(sendEntities.keySet(), receivedEntities.keySet());
            Assert.assertEquals(count, sendEntities.size());
        }
    }

    @Test(timeout = 1000)
    public void timeout() throws IpcException {
        final Duration readTimeoutMs = Duration.ofMillis(250);
        final long size = IpcChannel.getSuggestedSize(2000);
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment memory = arena.allocate(size);
            IpcChannel.create(memory, size);
            IpcChannel consumer = IpcChannel.connect(memory);

            long beforeRead = System.currentTimeMillis();
            try {
                consumer.read(readTimeoutMs);
                Assert.fail();
            } catch (IpcTimeoutException e) {}

            Assert.assertTrue(System.currentTimeMillis() - beforeRead >= readTimeoutMs.toMillis());
        }
    }
}
