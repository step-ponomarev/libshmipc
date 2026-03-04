package ipc.channel;

import ipc.exception.IpcException;
import ipc.exception.IpcUnknownException;
import ipc.status.IpcStatus;
import ipc.LibLoader;
import jextract.ipc_channel_h;
import jextract.ipc_entry_h;
import jextract.ipc_entry_t;
import jextract.ipc_error_t;

import java.io.Closeable;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.time.Duration;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public final class IpcChannel implements Closeable {
    static {
        LibLoader.load();
    }

    private final MemorySegment channel;
    private final ReadWriteLock lock = new ReentrantReadWriteLock();
    private boolean closed;

    private IpcChannel(MemorySegment channel) {
        this.channel = channel;
    }

    public static long getSuggestedSize(long desired) {
        return ipc_channel_h.ipc_channel_suggest_size(desired);
    }

    public static IpcChannel init(MemorySegment mem, long size) {
        if (mem == null) {
            throw new NullPointerException("mem is null");
        }

        if (size <= 0) {
            throw new IllegalArgumentException("size invalid size: " + size);
        }

        try (Arena tmpArena = Arena.ofConfined()) {
            final MemorySegment err = ipc_error_t.allocate(tmpArena);
            final MemorySegment outPtr = tmpArena.allocate(ValueLayout.ADDRESS);

            final IpcStatus status = IpcStatus.of(ipc_channel_h.ipc_channel_init(mem, size, outPtr, err));
            if (status == IpcStatus.IPC_STATUS_ERROR) {
                throw IpcException.from(err);
            }

            return new IpcChannel(outPtr.get(ValueLayout.ADDRESS, 0));
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnknownException(e);
        }
    }

    public static IpcChannel attach(MemorySegment mem) {
        if (mem == null) {
            throw new NullPointerException("mem is null");
        }

        try (Arena tmpArena = Arena.ofConfined()) {
            final MemorySegment err = ipc_error_t.allocate(tmpArena);
            final MemorySegment outPtr = tmpArena.allocate(ValueLayout.ADDRESS);

            final IpcStatus status = IpcStatus.of(ipc_channel_h.ipc_channel_attach(mem, outPtr, err));
            if (status == IpcStatus.IPC_STATUS_ERROR) {
                throw IpcException.from(err);
            }

            return new IpcChannel(outPtr.get(ValueLayout.ADDRESS, 0));
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnknownException(e);
        }
    }

    public IpcStatus write(byte[] bytes) {
        if (bytes == null) {
            throw new NullPointerException("bytes is null");
        }

        if (bytes.length == 0) {
            throw new IllegalArgumentException("empty byte array");
        }

        try (Arena tmpArena = Arena.ofConfined()) {
            final MemorySegment err = ipc_error_t.allocate(tmpArena);

            final IpcStatus status;
            lock.readLock().lock();
            try {
                if (closed) {
                    throw new IllegalStateException("channel is closed");
                }

                status = IpcStatus.of(ipc_channel_h.ipc_channel_write(
                                channel,
                                tmpArena.allocateFrom(ValueLayout.JAVA_BYTE, bytes),
                                bytes.length,
                                err
                        )
                );
            } finally {
                lock.readLock().unlock();
            }

            if (status == IpcStatus.IPC_STATUS_ERROR) {
                throw IpcException.from(err);
            }

            return status;
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnknownException(e);
        }
    }

    public byte[] read(Duration timeout) {
        if (timeout == null) {
            throw new NullPointerException("timeout is null");
        }

        try (Arena arena = Arena.ofConfined()) {
            final long start = System.nanoTime();
            final long timeNs = timeout.toNanos();

            long notify;
            lock.readLock().lock();
            try {
                if (closed) {
                    throw new IllegalStateException("channel is closed");
                }

                notify = ipc_channel_h.ipc_channel_get_notify_signal(this.channel);
            } finally {
                lock.readLock().unlock();
            }

            final MemorySegment err = ipc_error_t.allocate(arena);
            final MemorySegment entry = ipc_entry_t.allocate(arena);
            do {
                final IpcStatus status;
                lock.readLock().lock();
                try {
                    if (closed) {
                        throw new IllegalStateException("channel is closed");
                    }

                    status = IpcStatus.of(ipc_channel_h.ipc_channel_try_read(channel, entry, err));
                } finally {
                    lock.readLock().unlock();
                }

                if (status == IpcStatus.IPC_STATUS_ERROR) {
                    throw IpcException.from(err);
                }

                if (status == IpcStatus.IPC_STATUS_OK) {
                    try {
                        return ipcEntryToBytes(entry);
                    } finally {
                        ipc_entry_h.ipc_entry_destroy(entry);
                    }
                }

                long sleep = 10;
                while (true) { // TODO: measure and optimise, do not use native wait
                    final long spend = System.nanoTime() - start;
                    if (spend >= timeNs) {
                        return null;
                    }

                    long currNotify;
                    lock.readLock().lock();
                    try {
                        if (closed) {
                            throw new IllegalStateException("channel is closed");
                        }

                        currNotify = ipc_channel_h.ipc_channel_get_notify_signal(this.channel);
                    } finally {
                        lock.readLock().unlock();
                    }

                    if (currNotify != notify) {
                        notify = currNotify;
                        break;
                    }

                    //TODO fix owerflow
                    sleep = Math.min(Math.max(sleep * 2, sleep), timeNs - spend);
                    LockSupport.parkNanos(sleep); // virtual threads fix
                }
            } while (true);
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new IpcUnknownException(e);
        }
    }

    private static byte[] ipcEntryToBytes(MemorySegment ipcEntry) {
        final MemorySegment payload = ipc_entry_t.payload(ipcEntry);

        return payload.reinterpret(ipc_entry_t.size(ipcEntry)).toArray(ValueLayout.JAVA_BYTE);
    }

    @Override
    public void close() {
        lock.writeLock().lock();
        try (Arena tmpArena = Arena.ofConfined()) {
            if (closed) {
                return;
            }

            final MemorySegment err = ipc_error_t.allocate(tmpArena);
            if (IpcStatus.of(ipc_channel_h.ipc_channel_detach(channel, err)) != IpcStatus.IPC_STATUS_OK) {
                throw IpcException.from(err);
            }

            closed = true;
        } finally {
            lock.writeLock().unlock();
        }
    }
}
