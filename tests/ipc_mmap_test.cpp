#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "shmipc/ipc_common.h"
#include "shmipc/ipc_mmap.h"
#include <cstdint>
#include <cstdlib>
#include <unistd.h>

TEST_CASE("different segment sizes") {
    const char name[] = "/test";

    const IpcMemorySegmentResult created_segment = ipc_mmap(name, 1);
    CHECK(IpcMemorySegmentResult_is_ok(created_segment));

    const long page_size = sysconf(_SC_PAGESIZE);
    const IpcMemorySegmentResult diff_size_segment =
        ipc_mmap(name, page_size + 1);
    CHECK(IpcMemorySegmentResult_is_error(diff_size_segment));
    CHECK(ipc_unlink(created_segment.result).ipc_status == IPC_OK);
    CHECK(diff_size_segment.ipc_status == IPC_ERR_ILLEGAL_STATE);
}

TEST_CASE("min segment size") {
    const char name[] = "/test";

    const IpcMemorySegmentResult created_segment = ipc_mmap(name, 1);
    CHECK(IpcMemorySegmentResult_is_ok(created_segment));

    const long page_size = sysconf(_SC_PAGESIZE);
    const uint64_t size = created_segment.result->size;

    CHECK(ipc_unlink(created_segment.result).ipc_status == IPC_OK);
    CHECK(static_cast<uint64_t>(page_size) == size);
}
