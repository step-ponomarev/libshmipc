package lib.shm.ipc.benchmark;

import java.io.IOException;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.EnumSet;

public final class SharedMemoryFile implements AutoCloseable {
    private final Path path;
    private final FileChannel channel;
    private final MemorySegment segment;
    private final Arena arena;
    private final boolean owner;

    private SharedMemoryFile(Path path, FileChannel channel, MemorySegment segment, Arena arena, boolean owner) {
        this.path = path;
        this.channel = channel;
        this.segment = segment;
        this.arena = arena;
        this.owner = owner;
    }

    public static SharedMemoryFile create(Path path, long size) throws IOException {
        Arena arena = Arena.ofShared();

        FileChannel channel = FileChannel.open(path,
                EnumSet.of(
                        StandardOpenOption.CREATE,
                        StandardOpenOption.READ,
                        StandardOpenOption.WRITE,
                        StandardOpenOption.TRUNCATE_EXISTING
                ));

        channel.truncate(size);

        channel.write(ByteBuffer.allocate(1), size - 1);
        channel.force(true);

        MemorySegment segment = channel.map(FileChannel.MapMode.READ_WRITE, 0, size, arena);

        return new SharedMemoryFile(path, channel, segment, arena, true);
    }

    public static SharedMemoryFile open(Path path) throws IOException {
        Arena arena = Arena.ofShared();

        FileChannel channel = FileChannel.open(path,
                EnumSet.of(StandardOpenOption.READ, StandardOpenOption.WRITE));

        long size = channel.size();
        MemorySegment segment = channel.map(FileChannel.MapMode.READ_WRITE, 0, size, arena);

        return new SharedMemoryFile(path, channel, segment, arena, false);
    }

    public MemorySegment segment() {
        return segment;
    }

    public long size() {
        return segment.byteSize();
    }

    @Override
    public void close() throws IOException {
        arena.close();
        channel.close();
        if (owner) {
            Files.deleteIfExists(path);
        }
    }
}
