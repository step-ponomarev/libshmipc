package lib.shm.ipc.jextract;

import lib.shm.ipc.LibLoader;
import lib.shm.ipc.jextract.channel.*;
import org.junit.Assert;
import org.junit.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class ChannelBaseTest {
    @Test
    public void basicSingleThreadTest() {
        LibLoader.load();

        final long size = ipc_channel_h.ipc_channel_suggest_size(2000);
        System.out.println("Buffer size: " + size);

        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment buffer = arena.allocate(size);

            // Create — получаем Result, извлекаем канал
            MemorySegment createResult = ipc_channel_h.ipc_channel_create(arena, buffer, size);
            Assert.assertEquals("create failed", 0, IpcChannelCreateResult.ipc_status(createResult));
            MemorySegment producer = IpcChannelCreateResult.result(createResult);
            System.out.println("Producer: " + producer);

            // Connect — получаем Result, извлекаем канал
            MemorySegment connectResult = ipc_channel_h.ipc_channel_connect(arena, buffer);
            Assert.assertEquals("connect failed", 0, IpcChannelConnectResult.ipc_status(connectResult));
            MemorySegment consumer = IpcChannelConnectResult.result(connectResult);
            System.out.println("Consumer: " + consumer);

            // Write
            String testMsg = "Hello";
            byte[] bytes = testMsg.getBytes(StandardCharsets.UTF_8);
            MemorySegment msg = arena.allocateFrom(ValueLayout.JAVA_BYTE, bytes);

            MemorySegment writeResult = ipc_channel_h.ipc_channel_write(arena, producer, msg, bytes.length);
            int writeStatus = IpcChannelWriteResult.ipc_status(writeResult);
            System.out.println("Write status: " + writeStatus);
            Assert.assertEquals("write failed", 0, writeStatus);

            // Read
            MemorySegment entry = IpcEntry.allocate(arena);
            MemorySegment timeout = timespec.allocate(arena);
            timespec.tv_sec(timeout, 10);
            timespec.tv_nsec(timeout, 0L);

            MemorySegment readResult = ipc_channel_h.ipc_channel_read(arena, consumer, entry, timeout);
            int readStatus = IpcChannelReadResult.ipc_status(readResult);
            System.out.println("Read status: " + readStatus);
            Assert.assertEquals("read failed", 0, readStatus);
        }
    }

    @Test
    public void basicProducerConsumerTest() throws InterruptedException {
        LibLoader.load();

        final int count = 10000;
        final long size = ipc_channel_h.ipc_channel_suggest_size(1000);
        try (final Arena arena = Arena.ofShared();
             final ExecutorService exec = Executors.newVirtualThreadPerTaskExecutor()
        ) {
            final AtomicInteger received = new AtomicInteger(0);
            final MemorySegment buffer = arena.allocate(size);

            // Create — извлекаем канал из Result
            MemorySegment createResult = ipc_channel_h.ipc_channel_create(arena, buffer, size);
            Assert.assertEquals("create failed", 0, IpcChannelCreateResult.ipc_status(createResult));
            MemorySegment producer = IpcChannelCreateResult.result(createResult);

            // Connect — извлекаем канал из Result
            MemorySegment connectResult = ipc_channel_h.ipc_channel_connect(arena, buffer);
            Assert.assertEquals("connect failed", 0, IpcChannelConnectResult.ipc_status(connectResult));
            MemorySegment consumer = IpcChannelConnectResult.result(connectResult);

            final String messageTemplate = "Message %d";
            exec.execute(() -> {
                for (int i = 0; i < count; i++) {
                    String formatted = messageTemplate.formatted(i);
                    byte[] bytes = formatted.getBytes(StandardCharsets.UTF_8);
                    MemorySegment msg = arena.allocateFrom(ValueLayout.JAVA_BYTE, bytes);

                    MemorySegment resp = ipc_channel_h.ipc_channel_write(arena, producer, msg, bytes.length);
                    if (IpcChannelWriteResult.ipc_status(resp) != 0) {
                        i--;
                    }
                }
            });

            final MemorySegment timeout = timespec.allocate(arena);
            timespec.tv_sec(timeout, 100);
            timespec.tv_nsec(timeout, 0L);
            exec.execute(() -> {
                while (true) {
                    MemorySegment entry = IpcEntry.allocate(arena);

                    MemorySegment readResult = ipc_channel_h.ipc_channel_read(arena, consumer, entry, timeout);
                    int status = IpcChannelReadResult.ipc_status(readResult);
                    if (status != 0) {
                        continue;
                    }

                    final String expectedMessage = messageTemplate.formatted(received.getAndIncrement());
                    MemorySegment payload = IpcEntry.payload(entry);
                    long len = IpcEntry.size(entry);

                    byte[] messageBytes = payload.reinterpret(len).toArray(ValueLayout.JAVA_BYTE);
                    String message = new String(messageBytes, StandardCharsets.UTF_8);
                    Assert.assertEquals(expectedMessage, message);
                }
            });

            exec.shutdown();
            exec.awaitTermination(10, TimeUnit.SECONDS);
        }
    }
}
