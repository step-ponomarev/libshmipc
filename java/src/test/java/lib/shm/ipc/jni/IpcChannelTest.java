package lib.shm.ipc.jni;

import org.junit.Assert;
import org.junit.Test;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class IpcChannelTest {

    @Test
    public void simpleMessageCounterTest() throws InterruptedException, IOException {
        final Path mmapFile = Paths.get("/mmap.data");

        final int expectedMessageCount = 100;
        final AtomicInteger counter = new AtomicInteger(0);
        try (final ExecutorService executorService = Executors.newVirtualThreadPerTaskExecutor()) {
            final IpcChannel writter = new IpcChannel(mmapFile.toString(), 2000, true);
            final IpcChannel reader = new IpcChannel(mmapFile.toString(), 2000, false);
            executorService.submit(() -> {
                for (int i = 0; i < expectedMessageCount; i++) {
                    writter.write("Hello world!".getBytes(StandardCharsets.UTF_8));
                }
            });

            executorService.submit(() -> {

                for (int i = 0; i < expectedMessageCount; i++) {
                    byte[] read = reader.read();
                    counter.incrementAndGet();
                }

            });

            executorService.shutdown();
            executorService.awaitTermination(10, TimeUnit.SECONDS);

            Assert.assertEquals(expectedMessageCount, counter.get());
        } finally {
            Files.deleteIfExists(mmapFile);
        }
    }
}
