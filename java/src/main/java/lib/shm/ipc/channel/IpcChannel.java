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

    public static IpcChannel create(Arena arena, MemorySegment mem, long size) throws IpcSystemError {
        try {
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
            throw e;
        } catch (Exception e) {
            throw new IpcUnexpectedException(e);
        }
    }

    public static IpcChannel connect(Arena arena, MemorySegment mem) throws IpcSystemError {
        try {
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
            throw e;
        } catch (Exception e) {
            throw new IpcUnexpectedException(e);
        }
    }

    public void write(byte[] bytes) throws IpcSystemError, IpcWriteException {
        try {
            final MemorySegment writeResult = ipc_channel_h.ipc_channel_write(arena, channel, arena.allocateFrom(ValueLayout.JAVA_BYTE, bytes), bytes.length);

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

            throw new IpcUnexpectedException(errorMsg);
        } catch (IpcException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnexpectedException(e);
        }
    }

    public byte[] read(long timeoutMs) throws IpcReadException, IpcTimeoutException {
        try {
            final long start = System.currentTimeMillis();
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
        try {
            final MemorySegment entry = IpcEntry.allocate(arena);
            final MemorySegment tryReadResult = ipc_channel_h.ipc_channel_try_read(arena, channel, entry);
            IpcStatus ipcStatus = IpcStatus.of(IpcChannelTryReadResult.ipc_status(tryReadResult));
            if (ipcStatus == IpcStatus.IPC_OK) {
                return ipcEntryToBytes(entry);
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
        if (!arena.scope().isAlive()) {
            return;
        }

        arena.close();
    }
}
