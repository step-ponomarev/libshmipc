#!/bin/bash
set -e

MESSAGE_COUNT=${MESSAGE_COUNT:-100000}
WARMUP_COUNT=${WARMUP_COUNT:-10000}
MESSAGE_SIZE=${MESSAGE_SIZE:-64}
SHM_PATH="/tmp/shmipc_bench.shm"
UNIX_SOCKET_PATH="/tmp/shmipc_bench.sock"
TCP_PORT=9876

echo "=============================================="
echo "  libshmipc Java Benchmark Suite"
echo "=============================================="
echo ""
echo "Configuration:"
echo "  Messages:     $MESSAGE_COUNT"
echo "  Warmup:       $WARMUP_COUNT"
echo "  Message size: $MESSAGE_SIZE bytes"
echo ""

# Build benchmarks
echo "Building benchmarks..."
bazel build //benchmarks:java_benchmarks
echo ""

# Get paths to built binaries
PINGPONG_PRODUCER="bazel-bin/benchmarks/java/pingpong_producer"
PINGPONG_CONSUMER="bazel-bin/benchmarks/java/pingpong_consumer"
SOCKET_PINGPONG_SERVER="bazel-bin/benchmarks/java/socket_pingpong_server"
SOCKET_PINGPONG_CLIENT="bazel-bin/benchmarks/java/socket_pingpong_client"

cleanup() {
    echo "Cleaning up..."
    rm -f "$SHM_PATH"*
    rm -f "$UNIX_SOCKET_PATH"
    # Kill any background processes
    jobs -p | xargs -r kill 2>/dev/null || true
}
trap cleanup EXIT

ensure_runfiles_manifest() {
    local bin="$1"
    local runfiles_dir="${bin}.runfiles"
    local manifest="${bin}.runfiles_manifest"
    if [ -f "$manifest" ]; then
        mkdir -p "$runfiles_dir"
        ln -sf "$manifest" "$runfiles_dir/MANIFEST"
    fi
}

run_shm_pingpong() {
    echo "=============================================="
    echo "  Shared Memory IPC Ping-Pong"
    echo "=============================================="
    echo ""

    # Cleanup previous run
    rm -f "$SHM_PATH"*

    ensure_runfiles_manifest "$PINGPONG_PRODUCER"
    $PINGPONG_PRODUCER "$SHM_PATH" "$MESSAGE_COUNT" "$WARMUP_COUNT" "$MESSAGE_SIZE" &
    PRODUCER_PID=$!

    # Small delay to let producer create shared memory
    sleep 0.5

    ensure_runfiles_manifest "$PINGPONG_CONSUMER"
    $PINGPONG_CONSUMER "$SHM_PATH" "$MESSAGE_COUNT" "$WARMUP_COUNT" "$MESSAGE_SIZE"

    # Wait for producer to finish (it prints results)
    wait $PRODUCER_PID
    echo ""
}

run_tcp_pingpong() {
    echo "=============================================="
    echo "  TCP Socket Ping-Pong"
    echo "=============================================="
    echo ""

    ensure_runfiles_manifest "$SOCKET_PINGPONG_SERVER"
    $SOCKET_PINGPONG_SERVER tcp "$TCP_PORT" "$MESSAGE_COUNT" "$WARMUP_COUNT" "$MESSAGE_SIZE" &
    SERVER_PID=$!

    # Wait for server to start
    sleep 0.5

    ensure_runfiles_manifest "$SOCKET_PINGPONG_CLIENT"
    $SOCKET_PINGPONG_CLIENT tcp "localhost:$TCP_PORT" "$MESSAGE_COUNT" "$WARMUP_COUNT" "$MESSAGE_SIZE"

    # Wait for server to finish
    wait $SERVER_PID
    echo ""
}

run_unix_pingpong() {
    echo "=============================================="
    echo "  Unix Domain Socket Ping-Pong"
    echo "=============================================="
    echo ""

    # Cleanup previous socket
    rm -f "$UNIX_SOCKET_PATH"

    ensure_runfiles_manifest "$SOCKET_PINGPONG_SERVER"
    $SOCKET_PINGPONG_SERVER unix "$UNIX_SOCKET_PATH" "$MESSAGE_COUNT" "$WARMUP_COUNT" "$MESSAGE_SIZE" &
    SERVER_PID=$!

    # Wait for server to start
    sleep 0.5

    ensure_runfiles_manifest "$SOCKET_PINGPONG_CLIENT"
    $SOCKET_PINGPONG_CLIENT unix "$UNIX_SOCKET_PATH" "$MESSAGE_COUNT" "$WARMUP_COUNT" "$MESSAGE_SIZE"

    # Wait for server to finish
    wait $SERVER_PID
    echo ""
}

# Run benchmarks
case "${1:-all}" in
    shm)
        run_shm_pingpong
        ;;
    tcp)
        run_tcp_pingpong
        ;;
    unix)
        run_unix_pingpong
        ;;
    all)
        run_shm_pingpong
        run_tcp_pingpong
        run_unix_pingpong

        echo "=============================================="
        echo "  All benchmarks completed!"
        echo "=============================================="
        ;;
    help)
        echo "Usage: $0 [shm|tcp|unix|all]"
        echo ""
        echo "  shm  - SHM ping-pong benchmark"
        echo "  tcp  - TCP ping-pong benchmark"
        echo "  unix - Unix socket ping-pong benchmark"
        echo "  all  - All benchmarks (default)"
        exit 1
        ;;
esac
