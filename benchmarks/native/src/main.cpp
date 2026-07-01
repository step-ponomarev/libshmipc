#include <benchmark/benchmark.h>
#include <hdr/hdr_histogram.h>

using clk = std::chrono::steady_clock;

static int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        clk::now().time_since_epoch()).count();
}

static void BM_OddEven(benchmark::State &state) {
    struct hdr_histogram *histogram;
    hdr_init(1,
             LONG_MAX,
             5,
             &histogram
    );

    for (auto _: state) {
        int64_t t0 = now_ns();
        int64_t t1 = now_ns();
        int64_t diff = t1 - t0;

        hdr_record_value(
            histogram,
            diff
        );

        state.SetIterationTime(static_cast<double>(diff) / 1e9);
    }

    state.counters["p50"] = hdr_value_at_percentile(histogram, 50.0);
    state.counters["p99"] = hdr_value_at_percentile(histogram, 99.0);
    state.counters["p99.9"] = hdr_value_at_percentile(histogram, 99.9);
    state.counters["max"] = hdr_max(histogram);

    hdr_close(histogram);
}

BENCHMARK(BM_OddEven)->UseManualTime();
BENCHMARK_MAIN();