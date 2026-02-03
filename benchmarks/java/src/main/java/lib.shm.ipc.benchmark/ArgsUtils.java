package lib.shm.ipc.benchmark;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class ArgsUtils {
    public static final String ARG_ROLE = "role";
    public static final String ARG_MESSAGE_COUNT = "messageCount";
    public static final String ARG_WARMUP_COUNT = "warmupCount";
    public static final String ARG_MESSAGE_SIZE = "messageSize";
    public static final String ARG_BUFFER_SIZE = "bufferSize";

    private static final Pattern ARG_PATTERN = Pattern.compile("^--([a-zA-Z0-9_.-]+)=(.*)$");

    private ArgsUtils() {}

    public static Map<String, String> getArgs(String[] args) {
        if (args == null || args.length == 0) {
            return Collections.emptyMap();
        }

        final HashMap<String, String> params = new HashMap<>(args.length);
        for (String arg : args) {
            final Matcher m = ARG_PATTERN.matcher(arg);
            if (!m.matches()) {
                throw new IllegalArgumentException("Bad arg: " + arg + " expected --<key>=<value> pattern");
            }

            params.put(m.group(1), m.group(2));
        }

        return params;
    }
}
