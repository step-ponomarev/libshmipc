package lib.shm.ipc;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

public class LibLoader {
    private LibLoader() {}

    public static void load() {
        try {
            // Try to load from runfiles (when using runtime_deps in Bazel)
            String libPath = findLibraryInRunfiles();
            if (libPath != null) {
                System.load(libPath);
                return;
            }
            
            // Fallback to resource extraction (when packaged as resources)
            final String dir = "/native/" + os() + "-" + arch() + "/";
            final String fileName = os().equals("Darwin") ? "libshmipc_jni.dylib" : "libshmipc_jni.so";
            final Path corePath = extract(dir + fileName);
            System.load(corePath.toString());
        } catch (IOException e) {
            throw new UnsatisfiedLinkError("Failed to load native libs: " + e);
        }
    }
    
    private static String findLibraryInRunfiles() {
        // Check for runfiles directory from environment variables
        String runfilesDir = System.getenv("JAVA_RUNFILES");
        if (runfilesDir == null) {
            runfilesDir = System.getenv("RUNFILES_DIR");
        }
        if (runfilesDir == null) {
            // Try to find runfiles relative to the current class location
            String classPath = LibLoader.class.getProtectionDomain().getCodeSource().getLocation().getPath();
            // Look for .runfiles directory in the path
            int runfilesIdx = classPath.indexOf(".runfiles");
            if (runfilesIdx > 0) {
                runfilesDir = classPath.substring(0, runfilesIdx + ".runfiles".length());
            }
        }
        
        if (runfilesDir != null) {
            String fileName = os().equals("Darwin") ? "libshmipc_jni.dylib" : "libshmipc_jni.so";
            // Try common runfiles paths for runtime_deps
            String[] paths = {
                runfilesDir + "/_main/java/native/" + fileName,
                runfilesDir + "/java/native/" + fileName,
                runfilesDir + "/_main/" + fileName,  // Direct path if symlinked
            };
            for (String path : paths) {
                java.io.File file = new java.io.File(path);
                if (file.exists()) {
                    return file.getAbsolutePath();
                }
            }
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

    private static Path extract(String resPath) throws IOException {
        try (InputStream in = LibLoader.class.getResourceAsStream(resPath)) {
            if (in == null) throw new FileNotFoundException(resPath);
            Path tmp = Files.createTempFile("shmipc-", "-" + Paths.get(resPath).getFileName());
            tmp.toFile().deleteOnExit();
            Files.copy(in, tmp, StandardCopyOption.REPLACE_EXISTING);
            return tmp;
        }
    }
}
