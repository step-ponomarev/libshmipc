package ipc.exception;

import ipc.error.IpcCasTarget;
import ipc.error.IpcErrorCode;
import ipc.error.IpcErrorKind;
import jextract.ipc_error_cas_t;
import jextract.ipc_error_capacity_t;
import jextract.ipc_error_size_t;
import jextract.ipc_error_t;

import java.lang.foreign.MemorySegment;

public abstract class IpcException extends RuntimeException {
    public final IpcErrorKind kind;
    public final IpcErrorCode code;

    protected IpcException(IpcErrorKind kind, IpcErrorCode code, String message) {
        super(message);
        this.kind = kind;
        this.code = code;
    }

    public static IpcException from(MemorySegment err) {
        final IpcErrorKind kind = IpcErrorKind.of(ipc_error_t.kind(err));
        final IpcErrorCode code = IpcErrorCode.of(ipc_error_t.code(err));
        final String message = readMessage(ipc_error_t.message(err));
        final MemorySegment as = ipc_error_t.as(err);

        return switch (kind) {
            case IPC_ERR_KIND_ARG -> fromArg(code, message, ipc_error_t.as.arg(as));
            case IPC_ERR_KIND_SYS -> fromSys(code, message, ipc_error_t.as.sys(as));
            case IPC_ERR_KIND_INTERNAL -> fromInternal(code, message, ipc_error_t.as.internal(as));
            default -> throw new IllegalArgumentException("Unexpected error kind: " + kind);
        };
    }

    private static IpcException fromArg(IpcErrorCode code, String message, MemorySegment arg) {
        return switch (code) {
            case IPC_ERR_CODE_TOO_SMALL_SIZE, IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER -> {
                final MemorySegment size = ipc_error_t.as.arg.size(arg);
                yield new IpcArgSizeException(
                    code, message,
                    ipc_error_size_t.limit(size),
                    ipc_error_size_t.requested_size(size),
                    ipc_error_size_t.suggested_size(size)
                );
            }
            case IPC_ERR_CODE_INVALID_CAPACITY -> {
                final MemorySegment capacity = ipc_error_t.as.arg.capacity(arg);
                yield new IpcArgCapacityException(
                    message,
                    ipc_error_capacity_t.provided_capacity(capacity),
                    ipc_error_capacity_t.required_capacity(capacity)
                );
            }
            default -> new IpcArgException(code, message);
        };
    }

    private static IpcException fromSys(IpcErrorCode code, String message, MemorySegment sys) {
        return new IpcSysException(code, message, ipc_error_t.as.sys.sys_errno(sys));
    }

    private static IpcException fromInternal(IpcErrorCode code, String message, MemorySegment internal) {
        return switch (code) {
            case IPC_ERR_CODE_OFFSET_CAS_FAILED -> {
                final MemorySegment cas = ipc_error_t.as.internal.cas(internal);
                yield new IpcInternalCasException(
                    code, message,
                    IpcCasTarget.of(ipc_error_cas_t.target(cas)),
                    ipc_error_cas_t.expected_offset(cas),
                    ipc_error_cas_t.actual_offset(cas),
                    ipc_error_cas_t.desired_offset(cas)
                );
            }
            case IPC_ERR_CODE_ENTRY_CORRUPTED ->
                new IpcInternalCorruptedException(message, ipc_error_t.as.internal.offset(internal));
            default -> throw new IllegalArgumentException("Unexpected internal error code: " + code);
        };
    }

    private static String readMessage(MemorySegment msg) {
        if (msg.address() == 0) {
            return "unknown error";
        }
        return msg.getString(0);
    }
}
