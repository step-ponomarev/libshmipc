package lib.shm.ipc.channel;

import lib.shm.ipc.IpcStatus;
import lib.shm.ipc.LibLoader;
import lib.shm.ipc.exeption.*;
import jextract.*;
import lib.shm.ipc.exeption.IpcReadException;

import java.io.Closeable;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.time.Duration;

public final class IpcChannel implements Closeable {
    static {
        LibLoader.load();
    }

    private final Arena channelArena;
    private final MemorySegment channel;

    private IpcChannel(Arena channelArena, MemorySegment channel) {
        this.channelArena = channelArena;
        this.channel = channel;
    }

    public static long getSuggestedSize(long desired) {
        return ipc_channel_h.ipc_channel_suggest_size(desired);
    }

    public static IpcChannel create(MemorySegment mem, long size) throws IpcSystemError {
        Arena arena = null;
        try {
            arena = Arena.ofShared();
            final MemorySegment createResult = ipc_channel_h.ipc_channel_create(arena, mem, size);
            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelCreateResult.ipc_status(createResult));
            if (ipcStatus == IpcStatus.IPC_OK) {
                return new IpcChannel(arena, IpcChannelCreateResult.result(createResult));
            }

            final String errorMessage = parseErrorMessage(IpcChannelCreateResultError.detail(IpcChannelCreateResult.error(createResult)));
            if (ipcStatus == IpcStatus.IPC_ERR_SYSTEM) {
                throw new IpcSystemError(errorMessage);
            }

            throw new IpcUnexpectedException(errorMessage);
        } catch (IpcException e) {
            if (arena != null) {
                close(arena);
            }

            throw e;
        } catch (Exception e) {
            if (arena != null) {
                close(arena);
            }

            throw new IpcUnexpectedException(e);
        }
    }

    public static IpcChannel connect(MemorySegment mem) throws IpcSystemError {
        Arena arena = null;
        try {
            arena = Arena.ofShared();
            final MemorySegment createResult = ipc_channel_h.ipc_channel_connect(arena, mem);
            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelConnectResult.ipc_status(createResult));
            if (ipcStatus == IpcStatus.IPC_OK) {
                return new IpcChannel(arena, IpcChannelConnectResult.result(createResult));
            }

            final String errorMessage = parseErrorMessage(IpcChannelConnectResultError.detail(IpcChannelConnectResult.error(createResult)));
            if (ipcStatus == IpcStatus.IPC_ERR_SYSTEM) {
                throw new IpcSystemError(errorMessage);
            }

            throw new IpcUnexpectedException(errorMessage);
        } catch (IpcException e) {
            if (arena != null) {
                close(arena);
            }

            throw e;
        } catch (Exception e) {
            if (arena != null) {
                close(arena);
            }

            throw new IpcUnexpectedException(e);
        }
    }

    public void write(byte[] bytes) throws IpcSystemError, IpcLockedException, IpcWriteException {
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment data = arena.allocateFrom(ValueLayout.JAVA_BYTE, bytes);
            write(data, bytes.length);
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnexpectedException(e);
        }
    }

    public void write(MemorySegment data, long size) throws IpcSystemError, IpcLockedException, IpcWriteException {
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment writeResult = ipc_channel_h.ipc_channel_write(arena, channel, data, size);

            final IpcStatus ipcStatus = IpcStatus.of(IpcChannelWriteResult.ipc_status(writeResult));
            if (ipcStatus == IpcStatus.IPC_OK) {
                return;
            }

            final String errorMsg = parseErrorMessage(IpcChannelWriteResultError.detail(IpcChannelWriteResult.error(writeResult)));
            if (ipcStatus == IpcStatus.IPC_ERR_SYSTEM) {
                throw new IpcSystemError(errorMsg);
            }

            if (ipcStatus == IpcStatus.IPC_ERR_NO_SPACE_CONTIGUOUS) {
                throw new IpcWriteException(ipcStatus, errorMsg);
            }

            if (ipcStatus == IpcStatus.IPC_ERR_LOCKED) {
                throw new IpcLockedException(errorMsg);
            }

            throw new IpcUnexpectedException(errorMsg);
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnexpectedException(e);
        }
    }

    public byte[] read(Duration timeout) throws IpcReadException, IpcTimeoutException {
        try {
            final long start = System.currentTimeMillis();
            final long timeoutMs = timeout.toMillis();
            long notify = ipc_channel_h.ipc_channel_get_notify_signal(this.channel);
            byte[] res;

            do {
                if ((res = tryRead()) != null) {
                    return res;
                }

                while (true) {
                    if (System.currentTimeMillis() - start >= timeoutMs) {
                        throw new IpcTimeoutException("Read timed outed timeout %d".formatted(start));
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
            throw new IpcUnexpectedException(e);
        }
    }

    public byte[] tryRead() throws IpcReadException {
        try (final Arena arena = Arena.ofConfined()) {
            final MemorySegment readEntry = IpcEntry.allocate(arena);
            final MemorySegment tryReadResult = ipc_channel_h.ipc_channel_try_read(arena, channel, readEntry);
            IpcStatus ipcStatus = IpcStatus.of(IpcChannelTryReadResult.ipc_status(tryReadResult));
            if (ipcStatus == IpcStatus.IPC_OK) {
                return ipcEntryToBytes(readEntry);
            }

            if (ipc_channel_h.ipc_channel_is_retry_status(ipcStatus.getStatus())) {
                return null;
            }

            throw new IpcReadException(ipcStatus, parseErrorMessage(IpcChannelTryReadResultError.detail(IpcChannelTryReadResult.error(tryReadResult))));
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnexpectedException(e);
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
        close(channelArena);
    }

    private static void close(Arena arena) {
        if (!arena.scope().isAlive()) {
            return;
        }

        arena.close();
    }
}
