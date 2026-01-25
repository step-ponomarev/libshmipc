package lib.shm.ipc.result;


import lib.shm.ipc.IpcStatus;

public final class IpcResultWrapper<T> {
    private final IpcStatus status;
    private final T result;

    public IpcResultWrapper(IpcStatus status, T result) {
        this.status = status;
        this.result = result;
    }

    public IpcStatus getStatus() {
        return status;
    }

    public T getResult() {
        return result;
    }
}
