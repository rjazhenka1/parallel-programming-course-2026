#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>

struct Snapshot {
    std::array<uint64_t, 256> buckets;
    uint64_t count;
    uint64_t sum;
    uint64_t min;
    uint64_t max;
    uint64_t p50;
    uint64_t p99;
};

class MetricsCollector {
  public:
    virtual ~MetricsCollector() = default;
    virtual void record(uint64_t value) = 0;
    virtual Snapshot snapshot() = 0;
};

// Step 0 (single-threaded)

class STMetricsCollector : public MetricsCollector {
    std::array<uint64_t, 256> buckets_{};
    uint64_t sum_{0};
    uint64_t min_{std::numeric_limits<uint64_t>::max()};
    uint64_t max_{std::numeric_limits<uint64_t>::min()};
    // others can be calculated in snapshot()
  public:
    STMetricsCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};

// Step 1 (mutex)

class MutexMetricsCollector : public MetricsCollector {
    std::array<uint64_t, 256> buckets_{};
    uint64_t sum_{0};
    uint64_t min_{std::numeric_limits<uint64_t>::max()};
    uint64_t max_{std::numeric_limits<uint64_t>::min()};
    // others can be calculated in snapshot()
    std::mutex mutex_;

  public:
    MutexMetricsCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};

class EmptyMutexMetricsCollector : public MetricsCollector {
    std::mutex mutex_;

  public:
    EmptyMutexMetricsCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};
