package ipc.exception;

import ipc.error.IpcErrorCode;

// ipc_error_arg_capacity
public final class IpcArgCapacityException extends IpcArgException {
    public final long providedCapacity;
    public final long requiredCapacity;

    public IpcArgCapacityException(String message, long providedCapacity, long requiredCapacity) {
        super(IpcErrorCode.IPC_ERR_CODE_INVALID_CAPACITY, message);
        this.providedCapacity = providedCapacity;
        this.requiredCapacity = requiredCapacity;
    }
}
