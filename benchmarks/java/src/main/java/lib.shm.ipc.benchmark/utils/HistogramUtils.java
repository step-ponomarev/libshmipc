package lib.shm.ipc.benchmark.utils;

import lib.shm.ipc.benchmark.actors.LatencyResult;
import org.HdrHistogram.Histogram;

public class HistogramUtils {
    private HistogramUtils() {}

    public static LatencyResult toLatencyResult(Histogram hist) {
        return new LatencyResult(
                HistogramUtils.nsToUs(hist.getValueAtPercentile(50.0)),
                HistogramUtils.nsToUs(hist.getValueAtPercentile(95.0)),
                HistogramUtils.nsToUs(hist.getValueAtPercentile(99.0)),
                HistogramUtils.nsToUs(hist.getValueAtPercentile(99.99)),
                HistogramUtils.nsToUs(hist.getMinValue()),
                HistogramUtils.nsToUs(hist.getMaxValue())
        );
    }

    private static double nsToUs(long ns) {
        return (float) ns / 1_000.0;
    }
}
