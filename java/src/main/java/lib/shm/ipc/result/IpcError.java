package lib.shm.ipc.result;

public final class IpcError {
    public enum ErrorCode {
        UNKNOWN_ERROR(Integer.MIN_VALUE);
        private final int code;

        ErrorCode(int code) {
            this.code = code;
        }

        public static ErrorCode valueOf(int code) {
            for (var val : ErrorCode.values()) {
                if (val.code == code) {
                    return val;
                }
            }

            return UNKNOWN_ERROR;
        }

        public int getCode() {
            return code;
        }
    }

    private final ErrorCode code;
    private final String message;

    public IpcError(ErrorCode code, String message) {
        this.code = code;
        this.message = message;
    }

    public ErrorCode getCode() {
        return code;
    }

    public String getMessage() {
        return message;
    }
}
