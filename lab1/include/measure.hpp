#pragma once

#include "metrics_collector.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct MeasureResult {
    Snapshot collector_snapshot;
    double ops{};
};

MeasureResult measure_point(MetricsCollector &collector, const std::vector<uint64_t> &values,
                            const std::size_t thread_count);


struct ConsistencyResult {
    Snapshot collector_snapshot;

    uint64_t total_check_count{};
    uint64_t check_equal_count{};
    uint64_t check_less_count{};
    uint64_t check_greater_count{};

    uint64_t true_call_count{};
};

ConsistencyResult consistency_check(MetricsCollector &collector, const std::vector<uint64_t> &values,
                            const std::size_t thread_count);
