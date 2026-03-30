package lib.shm.ipc.benchmark.utils;

import java.nio.file.Path;

public final class PathUtils {
    private PathUtils() {}

    public static Path inPath(String base) {
        return Path.of(base + ".in");
    }

    public static Path outPath(String base) {
        return Path.of(base + ".out");
    }
}
