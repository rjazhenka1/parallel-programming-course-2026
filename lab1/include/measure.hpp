#pragma once

#include "metrics_collector.hpp"

#include <cstddef>
#include <vector>

struct MeasureResult {
    Snapshot collector_snapshot;
    double ops;
};

MeasureResult measure_point(MetricsCollector &collector, const std::vector<uint64_t> &values,
                            const std::size_t thread_count);
