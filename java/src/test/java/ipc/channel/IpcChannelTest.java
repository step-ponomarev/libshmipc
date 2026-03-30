package ipc.channel;

import ipc.status.IpcStatus;
import org.junit.Assert;
import org.junit.Test;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

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

    @Test(timeout = 60000)
    public void basicProducerConsumerTest() throws Throwable {
        final int count = 1_000_000;
        final long size = IpcChannel.getSuggestedSize(200);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final AtomicInteger received = new AtomicInteger(0);
            final AtomicReference<Throwable> error = new AtomicReference<>();
            final MemorySegment memory = arena.allocate(size);
            try (IpcChannel producer = IpcChannel.init(memory, size);
                 IpcChannel consumer = IpcChannel.attach(memory)) {

                final String messageTemplate = "Message %d";
                exec.execute(() -> {
                    try {
                        for (int i = 0; i < count && error.get() == null; i++) {
                            String formatted = messageTemplate.formatted(i);
                            byte[] bytes = formatted.getBytes(StandardCharsets.UTF_8);
                            if (producer.write(bytes) != IpcStatus.IPC_STATUS_OK) {
                                i--;
                            }
                        }
                    } catch (Throwable t) {
                        error.compareAndSet(null, t);
                    }
                });

                exec.execute(() -> {
                    try {
                        while (error.get() == null) {
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
                    } catch (Throwable t) {
                        error.compareAndSet(null, t);
                    }
                });

                exec.shutdown();
                if (!exec.awaitTermination(60, TimeUnit.SECONDS)) {
                    error.compareAndSet(null, new AssertionError("Test timed out"));
                }
            }

            final Throwable t = error.get();
            if (t != null) {
                throw t;
            }
        }
    }

    @Test(timeout = 60000)
    public void basicMultiProducerMultiConsumerTest() throws Throwable {
        final int count = 10_000;
        final long size = IpcChannel.getSuggestedSize(200);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final MemorySegment memory = arena.allocate(size);
            final AtomicInteger send = new AtomicInteger(0);
            final AtomicReference<Throwable> error = new AtomicReference<>();
            final ConcurrentHashMap<String, Object> sendEntities = new ConcurrentHashMap<>();
            final String messageTemplate = "Message %d";

            IpcChannel.init(memory, size).close();
            for (int i = 0; i < 2; i++) {
                IpcChannel producer = IpcChannel.attach(memory);
                exec.execute(() -> {
                    try {
                        while (error.get() == null) {
                            final int num = send.getAndIncrement();
                            if (num >= count) {
                                break;
                            }

                            final String msg = messageTemplate.formatted(num);
                            while (error.get() == null) {
                                if (producer.write(msg.getBytes(StandardCharsets.UTF_8)) == IpcStatus.IPC_STATUS_OK) {
                                    sendEntities.put(msg, STUB);
                                    break;
                                }
                            }
                        }
                    } catch (Throwable t) {
                        error.compareAndSet(null, t);
                    }
                });
            }

            final ConcurrentHashMap<String, Object> receivedEntities = new ConcurrentHashMap<>();
            for (int i = 0; i < 10; i++) {
                IpcChannel consumer = IpcChannel.attach(memory);
                exec.execute(() -> {
                    try {
                        while (error.get() == null && receivedEntities.size() != count) {
                            byte[] readResult = consumer.read(Duration.ofSeconds(1));
                            if (readResult != null) {
                                String message = new String(readResult, StandardCharsets.UTF_8);
                                receivedEntities.put(message, STUB);
                            }
                        }
                    } catch (Throwable t) {
                        error.compareAndSet(null, t);
                    }
                });
            }

            exec.shutdown();
            if (!exec.awaitTermination(60, TimeUnit.SECONDS)) {
                error.compareAndSet(null, new AssertionError("Test timed out"));
            }

            Throwable t = error.get();
            if (t != null) {
                throw t;
            }

            Assert.assertEquals(sendEntities.keySet(), receivedEntities.keySet());
            Assert.assertEquals(count, sendEntities.size());
        }
    }

    @Test(timeout = 1000)
    public void timeout() {
        final Duration readTimeoutMs = Duration.ofMillis(250);
        final long size = IpcChannel.getSuggestedSize(2000);
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment memory = arena.allocate(size);
            try (IpcChannel ignored = IpcChannel.init(memory, size);
                 IpcChannel consumer = IpcChannel.attach(memory)) {

                long beforeRead = System.nanoTime();
                byte[] result = consumer.read(readTimeoutMs);
                Assert.assertNull(result);
                Assert.assertTrue(System.nanoTime() - beforeRead >= readTimeoutMs.toNanos());
            }
        }
    }

    @Test(timeout = 60000)
    public void noNativeMemoryLeak() throws Exception {
        final int messageSize = 256;
        final byte[] data = new byte[messageSize];

        final int iterations = 1_000_000;
        final long rssBefore = getProcessRssKb();
        try (Arena arena = Arena.ofShared()) {
            MemorySegment memory = arena.allocate(IpcChannel.getSuggestedSize(4096));
            try (IpcChannel producer = IpcChannel.init(memory, memory.byteSize());
                 IpcChannel consumer = IpcChannel.attach(memory)) {
                for (int i = 0; i < iterations; i++) {
                    producer.write(data);
                    consumer.read(Duration.ofMillis(20));
                }
            }
        }

        System.gc();
        Thread.sleep(200);

        final long rssAfter = getProcessRssKb();
        final long rssGrowthKb = rssAfter - rssBefore;
        final long maxAllowedGrowthKb = 20 * 1024; // 20kb

        Assert.assertTrue(
                String.format("Native memory leak detected: RSS grew by %d KB (max allowed: %d KB)",
                        rssGrowthKb, maxAllowedGrowthKb),
                rssGrowthKb < maxAllowedGrowthKb
        );
    }

    private static long getProcessRssKb() throws Exception {
        final long pid = ProcessHandle.current().pid();
        final String os = System.getProperty("os.name").toLowerCase();

        final Process process;
        if (os.contains("mac") || os.contains("linux")) {
            process = Runtime.getRuntime().exec(new String[]{"ps", "-o", "rss=", "-p", String.valueOf(pid)});
        } else {
            throw new IllegalStateException("Unsupported OS: " + os);
        }

        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            String line = reader.readLine();
            if (line == null || line.isBlank()) {
                throw new RuntimeException("Failed to get RSS");
            }
            return Long.parseLong(line.trim());
        }
    }
}
