package lib.shm.ipc;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

public class LibLoader {
    private LibLoader() {}

    public static void load() {
        String resPath = "/native/" + os() + "-" + arch() + "/" + (os().equals("Darwin") ? "libshmipc_jni.dylib" : "libshmipc_jni.so");
        try (InputStream in = LibLoader.class.getResourceAsStream(resPath)) {
            if (in != null) {
                Path tmp = Files.createTempFile("shmipc-", "-" + Paths.get(resPath).getFileName());
                tmp.toFile().deleteOnExit();
                Files.copy(in, tmp, StandardCopyOption.REPLACE_EXISTING);
                System.load(tmp.toString());
                return;
            }
        } catch (IOException e) {}
        
        // Fallback to runfiles (local run)
        String libPath = findLibraryInRunfiles();
        if (libPath != null) {
            System.load(libPath);
            return;
        }
        
        throw new UnsatisfiedLinkError("Native library not found in resources or runfiles. Resource path: " + resPath);
    }

    private static String findLibraryInRunfiles() {
        String runfilesDir = System.getenv("JAVA_RUNFILES");
        if (runfilesDir == null) {
            runfilesDir = System.getenv("RUNFILES_DIR");
        }
        if (runfilesDir == null) {
            String classPath = LibLoader.class.getProtectionDomain().getCodeSource().getLocation().getPath();
            int runfilesIdx = classPath.indexOf(".runfiles");
            if (runfilesIdx > 0) {
                runfilesDir = classPath.substring(0, runfilesIdx + ".runfiles".length());
            }
        }
        
        if (runfilesDir == null) {
            return null;
        }
        
        String fileName = os().equals("Darwin") ? "libshmipc_jni.dylib" : "libshmipc_jni.so";
        Path libPath = Paths.get(runfilesDir, "_main", "java", "native", fileName);
        
        if (Files.exists(libPath)) {
            return libPath.toAbsolutePath().toString();
        }
        
        return null;
    }

    private static String os() {
        final String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("mac")) {
            return "Darwin";
        }

        if (os.contains("linux")) {
            return "Linux";
        }

        throw new UnsupportedOperationException("OS: " + os);
    }

    private static String arch() {
        String arch = System.getProperty("os.arch").toLowerCase();
        if (arch.equals("x86_64") || arch.equals("amd64")) {
            return "x86_64";
        }
        if (arch.equals("aarch64") || arch.equals("arm64")) {
            return os().equals("Linux") ? "aarch64" : "arm64";
        }

        throw new UnsupportedOperationException("Arch: " + arch);
    }
}
