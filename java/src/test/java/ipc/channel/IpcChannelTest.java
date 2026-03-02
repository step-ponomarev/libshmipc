package ipc.channel;

import ipc.status.IpcStatus;
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
    public void basicSingleThreadTest() {
        final long size = IpcChannel.getSuggestedSize(2000);
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment memory = arena.allocate(size);
            try (IpcChannel producer = IpcChannel.init(memory, size);
                 IpcChannel consumer = IpcChannel.attach(memory)) {

                String testMsg = "Hello";
                byte[] bytes = testMsg.getBytes(StandardCharsets.UTF_8);

                producer.write(bytes);

                byte[] readResult = consumer.read(Duration.ofMillis(200));
                Assert.assertNotNull(readResult);
                Assert.assertEquals(testMsg, new String(readResult, StandardCharsets.UTF_8));
            }
        }
    }

    @Test
    public void basicProducerConsumerTest() throws InterruptedException {
        final int count = 1_000_000;
        final long size = IpcChannel.getSuggestedSize(200);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final AtomicInteger received = new AtomicInteger(0);
            final MemorySegment memory = arena.allocate(size);
            try (IpcChannel producer = IpcChannel.init(memory, size);
                 IpcChannel consumer = IpcChannel.attach(memory)) {

                final String messageTemplate = "Message %d";
                exec.execute(() -> {
                    for (int i = 0; i < count; i++) {
                        String formatted = messageTemplate.formatted(i);
                        byte[] bytes = formatted.getBytes(StandardCharsets.UTF_8);
                        if (producer.write(bytes) != IpcStatus.IPC_STATUS_OK) {
                            i--;
                        }
                    }
                });

                exec.execute(() -> {
                    while (true) {
                        byte[] readResult = consumer.read(Duration.ofSeconds(1));
                        if (readResult != null) {
                            final String expectedMessage = messageTemplate.formatted(received.getAndIncrement());
                            String message = new String(readResult, StandardCharsets.UTF_8);
                            Assert.assertEquals(expectedMessage, message);
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
    }
//
//    @Test(timeout = 60000)
//    public void basicMultiProducerMultiConsumerTest() throws InterruptedException {
//        final int count = 100_000;
//        final long size = IpcChannel.getSuggestedSize(200);
//        try (final Arena arena = Arena.ofShared();
//             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
//        ) {
//            final MemorySegment memory = arena.allocate(size);
//            final AtomicInteger send = new AtomicInteger(0);
//            final ConcurrentHashMap<String, Object> sendEntities = new ConcurrentHashMap<>();
//            final String messageTemplate = "Message %d";
//
//            IpcChannel.init(memory, size).close();
//            for (int i = 0; i < 2; i++) {
//                IpcChannel producer = IpcChannel.attach(memory);
//                exec.execute(() -> {
//                    while (true) {
//                        final int num = send.getAndIncrement();
//                        if (num >= count) {
//                            break;
//                        }
//
//                        final String msg = messageTemplate.formatted(num);
//                        while (true) {
//                            if (producer.write(msg.getBytes(StandardCharsets.UTF_8)) == IpcStatus.IPC_STATUS_OK) {
//                                sendEntities.put(msg, STUB);
//                                break;
//                            }
//                        }
//                    }
//                });
//            }
//
//            final ConcurrentHashMap<String, Object> receivedEntities = new ConcurrentHashMap<>();
//            for (int i = 0; i < 10; i++) {
//                IpcChannel consumer = IpcChannel.attach(memory);
//                exec.execute(() -> {
//                    while (receivedEntities.size() != count) {
//                        byte[] readResult = consumer.read(Duration.ofSeconds(1));
//                        if (readResult != null) {
//                            String message = new String(readResult, StandardCharsets.UTF_8);
//                            receivedEntities.put(message, STUB);
//                        }
//                    }
//                });
//            }
//
//            exec.shutdown();
//            exec.awaitTermination(10, TimeUnit.SECONDS);
//            Assert.assertEquals(sendEntities.keySet(), receivedEntities.keySet());
//            Assert.assertEquals(count, sendEntities.size());
//        }
//    }
//
//    @Test(timeout = 1000)
//    public void timeout() {
//        final Duration readTimeoutMs = Duration.ofMillis(250);
//        final long size = IpcChannel.getSuggestedSize(2000);
//        try (final Arena arena = Arena.ofConfined()) {
//            final MemorySegment memory = arena.allocate(size);
//            try (IpcChannel ignored = IpcChannel.init(memory, size);
//                 IpcChannel consumer = IpcChannel.attach(memory)) {
//
//                long beforeRead = System.currentTimeMillis();
//                byte[] result = consumer.read(readTimeoutMs);
//                Assert.assertNull(result);
//                Assert.assertTrue(System.currentTimeMillis() - beforeRead >= readTimeoutMs.toMillis());
//            }
//        }
//    }
}
