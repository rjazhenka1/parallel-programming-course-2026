#include "metrics_collector.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numeric>

namespace {

uint64_t find_percentile(const std::array<uint64_t, 256> &buckets, uint64_t count, double percentile) {
    uint64_t threshold = count * percentile;
    uint64_t total = 0;
    for (std::size_t i = 0; i < buckets.size(); i++) {
        const auto val = buckets[i];

        total += val;
        if (total >= threshold) {
            return i * 4;
        }
    }

    return 255 * 4;
}

} // namespace

// Step 0

void STMetricsCollector::record(uint64_t value) {
    std::size_t bucket_index = std::min(value / 4, uint64_t{255});
    buckets_[bucket_index]++;
    sum_ += value;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
}

Snapshot STMetricsCollector::snapshot() {
    uint64_t count = std::accumulate(buckets_.begin(), buckets_.end(), uint64_t{0});

    uint64_t p50 = find_percentile(buckets_, count, 0.5);
    uint64_t p99 = find_percentile(buckets_, count, 0.99);

    Snapshot snapshot{
        .buckets = buckets_,
        .count = count,
        .sum = sum_,
        .min = min_,
        .max = max_,
        .p50 = p50,
        .p99 = p99,
    };

    return snapshot;
}

// Step 1

void MutexMetricsCollector::record(uint64_t value) {
    std::size_t bucket_index = std::min(value / 4, uint64_t{255}); // no mutex needed here

    std::unique_lock<std::mutex> lock{mutex_};
    buckets_[bucket_index]++;
    sum_ += value;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
}

Snapshot MutexMetricsCollector::snapshot() {
    std::unique_lock<std::mutex> lock{mutex_};

    uint64_t count = std::accumulate(buckets_.begin(), buckets_.end(), uint64_t{0});

    uint64_t p50 = find_percentile(buckets_, count, 0.5);
    uint64_t p99 = find_percentile(buckets_, count, 0.99);

    Snapshot snapshot{
        .buckets = buckets_,
        .count = count,
        .sum = sum_,
        .min = min_,
        .max = max_,
        .p50 = p50,
        .p99 = p99,
    };

    return snapshot;
}

void EmptyMutexMetricsCollector::record(uint64_t value) { std::unique_lock<std::mutex> lock{mutex_}; }

Snapshot EmptyMutexMetricsCollector::snapshot() {
    std::unique_lock<std::mutex> lock{mutex_};
    return Snapshot{};
}
