package lib.shm.ipc.channel;

import lib.shm.ipc.IpcStatus;
import lib.shm.ipc.LibLoader;
import lib.shm.ipc.exeption.IpcException;
import jextract.*;
import lib.shm.ipc.result.IpcResultWrapper;

import java.io.Closeable;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;

// TODO: Prepare exceptions
//TODO: не палить статусы на ружу.
// TODO: либо результат либо соотв ошибка
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

    public static IpcResultWrapper<IpcChannel> create(Arena arena, MemorySegment mem, long size) throws IpcException {
        try {
            final MemorySegment createResult = ipc_channel_h.ipc_channel_create(arena, mem, size);
            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelCreateResult.ipc_status(createResult));

            if (ipcStatus != IpcStatus.IPC_OK) {
                throw new IpcException(ipcStatus, parseErrorMessage(IpcChannelCreateResultError.detail(IpcChannelCreateResult.error(createResult))));
            }

            return new IpcResultWrapper<>(ipcStatus, new IpcChannel(arena, IpcChannelCreateResult.result(createResult)));
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcException(IpcStatus.IPC_ERR_SYSTEM, e);
        }
    }

    public static IpcResultWrapper<IpcChannel> connect(Arena arena, MemorySegment mem) throws IpcException {
        try {
            final MemorySegment createResult = ipc_channel_h.ipc_channel_connect(arena, mem);
            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelConnectResult.ipc_status(createResult));

            if (ipcStatus != IpcStatus.IPC_OK) {
                throw new IpcException(ipcStatus, parseErrorMessage(IpcChannelConnectResultError.detail(IpcChannelConnectResult.error(createResult))));
            }

            return new IpcResultWrapper<>(ipcStatus, new IpcChannel(arena, IpcChannelConnectResult.result(createResult)));
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcException(IpcStatus.IPC_ERR_SYSTEM, e);
        }
    }

    public IpcResultWrapper<Void> write(byte[] bytes) throws IpcException {
        try {
            final MemorySegment writeResult = ipc_channel_h.ipc_channel_write(arena, channel, arena.allocateFrom(ValueLayout.JAVA_BYTE, bytes), bytes.length);

            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelWriteResult.ipc_status(writeResult));
            if (ipcStatus != IpcStatus.IPC_OK) {
                throw new IpcException(ipcStatus, parseErrorMessage(IpcChannelWriteResultError.detail(IpcChannelWriteResult.error(writeResult))));
            }

            return new IpcResultWrapper<>(ipcStatus, null);
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcException(IpcStatus.IPC_ERR_SYSTEM, e);
        }
    }

    public IpcResultWrapper<byte[]> read(long timeoutMs) throws IpcException {
        try {
            final long start = System.currentTimeMillis();
            long notify = ipc_channel_h.ipc_channel_get_notify_signal(this.channel);

            do {
                try {
                    IpcResultWrapper<byte[]> readResult = tryRead();
                    if (readResult.getStatus() == IpcStatus.IPC_OK) {
                        return readResult;
                    }
                } catch (IpcException e) {
                    if (!ipc_channel_h.ipc_channel_is_retry_status(e.getStatus().getStatus())) {
                        throw e;
                    }
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
            } while (true);
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcException(IpcStatus.IPC_ERR_SYSTEM, e);
        }
    }

    public IpcResultWrapper<byte[]> tryRead() throws IpcException {
        try {
            final MemorySegment entry = IpcEntry.allocate(arena);
            final MemorySegment tryReadResult = ipc_channel_h.ipc_channel_try_read(arena, channel, entry);
            IpcStatus ipcStatus = IpcStatus.of(IpcChannelTryReadResult.ipc_status(tryReadResult));
            if (ipcStatus == IpcStatus.IPC_OK) {
                return new IpcResultWrapper<>(ipcStatus, ipcEntryToBytes(entry));
            }

            if (ipcStatus == IpcStatus.IPC_EMPTY) {
                return new IpcResultWrapper<>(ipcStatus, null);
            }

            throw new IpcException(ipcStatus, parseErrorMessage(IpcChannelTryReadResultError.detail(IpcChannelTryReadResult.error(tryReadResult))));
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcException(IpcStatus.IPC_ERR_SYSTEM, e);
        }
    }

    private static byte[] ipcEntryToBytes(MemorySegment ipcEntry) {
        final MemorySegment payload = IpcEntry.payload(ipcEntry);

        return payload.reinterpret(IpcEntry.size(ipcEntry)).toArray(ValueLayout.JAVA_BYTE);
    }

    private static String parseErrorMessage(MemorySegment msg) {
        if (msg.address() == 0) {
            return "Something wrong";
        }

        return msg.getString(0);
    }

    @Override
    public synchronized void close() {
        if (!arena.scope().isAlive()) {
            return;
        }

        arena.close();
    }
}
