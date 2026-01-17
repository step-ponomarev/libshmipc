package lib.shm.ipc.channel;

import lib.shm.ipc.IpcStatus;
import lib.shm.ipc.LibLoader;
import lib.shm.ipc.jextract.channel.*;
import lib.shm.ipc.result.IpcResultWrapper;

import java.io.Closeable;

import java.io.IOException;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;

// TODO: Prepare exceptions
public final class IpcChannel implements Closeable {
    static {
        LibLoader.load();
    }

    private final Arena arena;
    private final MemorySegment channel;

    private IpcChannel(Arena arena, MemorySegment channel) {
        this.arena = arena;
        this.channel = channel;
    }

    public static long getSuggestedSize(long desired) {
        return ipc_channel_h.ipc_channel_suggest_size(desired);
    }

    public static IpcResultWrapper<IpcChannel> create(Arena arena, MemorySegment mem, long size) {
        try {
            final MemorySegment createResult = ipc_channel_h.ipc_channel_create(arena, mem, size);
            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelCreateResult.ipc_status(createResult));
            if (ipcStatus != IpcStatus.IPC_OK) {
                return new IpcResultWrapper<>(ipcStatus, null);
            }

            return new IpcResultWrapper<>(ipcStatus, new IpcChannel(arena, IpcChannelCreateResult.result(createResult)));
        } catch (Exception e) {
            return new IpcResultWrapper<>(IpcStatus.IPC_ERR_SYSTEM, null);
        }
    }

    public static IpcResultWrapper<IpcChannel> connect(Arena arena, MemorySegment mem) {
        try {
            final MemorySegment createResult = ipc_channel_h.ipc_channel_connect(arena, mem);
            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelConnectResult.ipc_status(createResult));
            if (ipcStatus != IpcStatus.IPC_OK) {
                return new IpcResultWrapper<>(ipcStatus, null);
            }

            return new IpcResultWrapper<>(ipcStatus, new IpcChannel(arena, IpcChannelConnectResult.result(createResult)));
        } catch (Exception e) {
            return new IpcResultWrapper<>(IpcStatus.IPC_ERR_SYSTEM, null);
        }
    }

    public IpcResultWrapper<Void> write(byte[] bytes) {
        MemorySegment writeResult = ipc_channel_h.ipc_channel_write(arena, channel, arena.allocateFrom(ValueLayout.JAVA_BYTE, bytes), bytes.length);
        return new IpcResultWrapper<>(IpcStatus.of(IpcChannelWriteResult.ipc_status(writeResult)), null);
    }

    //TODO: в чем таймаут?
    public IpcResultWrapper<byte[]> read(long timeoutMs) {
        // TODO: try catch etc
        final long start = System.currentTimeMillis();
        long notify = ipc_channel_h.ipc_channel_get_notify_signal(this.channel);

        IpcResultWrapper<byte[]> readResult;
        do {
            readResult = tryRead();
            if (readResult.getStatus() == IpcStatus.IPC_OK || !ipc_channel_h.ipc_channel_is_retry_status(readResult.getStatus().getStatus())) {
                break;
            }

            while (true) {
                if (System.currentTimeMillis() - start >= timeoutMs) {
                    return new IpcResultWrapper<>(IpcStatus.IPC_ERR_TIMEOUT, null);
                }

                long currNotify = ipc_channel_h.ipc_channel_get_notify_signal(this.channel);
                if (currNotify != notify) {
                    notify = currNotify;
                    break;
                }
                Thread.onSpinWait();
            }
        } while (System.currentTimeMillis() - start < timeoutMs);

        return readResult;
    }

    public IpcResultWrapper<byte[]> tryRead() {
        final MemorySegment entry = IpcEntry.allocate(arena);
        final MemorySegment result = ipc_channel_h.ipc_channel_try_read(arena, channel, entry);
        IpcStatus ipcStatus = IpcStatus.of(IpcChannelTryReadResult.ipc_status(result));
        if (ipcStatus != IpcStatus.IPC_OK) {
            new IpcResultWrapper<>(ipcStatus, null);
        }

        return new IpcResultWrapper<>(ipcStatus, ipcEntryToBytes(entry));
    }

    private static byte[] ipcEntryToBytes(MemorySegment ipcEntry) {
        final MemorySegment payload = IpcEntry.payload(ipcEntry);

        return payload.reinterpret(IpcEntry.size(ipcEntry))
                .toArray(ValueLayout.JAVA_BYTE);
    }

    @Override
    public void close() throws IOException {
        this.arena.close(); // TODO: handle a lot of exceptions
    }
}
