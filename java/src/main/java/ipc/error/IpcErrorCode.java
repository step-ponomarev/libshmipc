package ipc.error;

import jextract.ipc_error_h;

public enum IpcErrorCode {
    IPC_ERR_CODE_NONE(ipc_error_h.IPC_ERR_CODE_NONE()),

    IPC_ERR_CODE_NULL_ARG(ipc_error_h.IPC_ERR_CODE_NULL_ARG()),
    IPC_ERR_CODE_TOO_SMALL_SIZE(ipc_error_h.IPC_ERR_CODE_TOO_SMALL_SIZE()),
    IPC_ERR_CODE_ZERO_SIZE(ipc_error_h.IPC_ERR_CODE_ZERO_SIZE()),
    IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER(ipc_error_h.IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER()),
    IPC_ERR_CODE_INVALID_CAPACITY(ipc_error_h.IPC_ERR_CODE_INVALID_CAPACITY()),
    IPC_ERR_CODE_INVALID_TIMEOUT(ipc_error_h.IPC_ERR_CODE_INVALID_TIMEOUT()),

    IPC_ERR_CODE_OFFSET_CAS_FAILED(ipc_error_h.IPC_ERR_CODE_OFFSET_CAS_FAILED()),
    IPC_ERR_CODE_ENTRY_CORRUPTED(ipc_error_h.IPC_ERR_CODE_ENTRY_CORRUPTED()),

    IPC_ERR_CODE_ALLOCATION(ipc_error_h.IPC_ERR_CODE_ALLOCATION()),
    IPC_ERR_CODE_GET_TIME(ipc_error_h.IPC_ERR_CODE_GET_TIME()),
    IPC_ERR_CODE_FUTEX_WAIT(ipc_error_h.IPC_ERR_CODE_FUTEX_WAIT());

    public final int value;

    IpcErrorCode(int value) {
        this.value = value;
    }

    public static IpcErrorCode of(int value) {
        for (var c : values()) {
            if (c.value == value) {
                return c;
            }
        }
        throw new IllegalArgumentException("Unknown ipc_error_code_t: " + value);
    }
}