#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

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
    uint64_t count_{0};
    uint64_t sum_{0};
    uint64_t min_{std::numeric_limits<uint64_t>::max()};
    uint64_t max_{std::numeric_limits<uint64_t>::min()};

  public:
    STMetricsCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};

// Step 1 (mutex)

class MutexMetricsCollector : public MetricsCollector {
    std::array<uint64_t, 256> buckets_{};
    uint64_t count_{0};
    uint64_t sum_{0};
    uint64_t min_{std::numeric_limits<uint64_t>::max()};
    uint64_t max_{std::numeric_limits<uint64_t>::min()};
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

// Step 2 (Lock Striping)

class StripedMutexMetricsCollector : public MetricsCollector {
    std::array<uint64_t, 256> buckets_{};
    std::atomic<uint64_t> count_{0};
    std::atomic<uint64_t> sum_{0};
    std::atomic<uint64_t> min_{std::numeric_limits<uint64_t>::max()};
    std::atomic<uint64_t> max_{std::numeric_limits<uint64_t>::min()};
    std::array<std::mutex, 16> mutexes_{};

  public:
    StripedMutexMetricsCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};

// Step 3 (thread-local)

struct alignas(64) ThreadState {
    std::array<std::atomic<uint64_t>, 256> buckets{};
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> sum{0};
    std::atomic<uint64_t> min{std::numeric_limits<uint64_t>::max()};
    std::atomic<uint64_t> max{0};
};

inline uint64_t next_collector_id() {
    static std::atomic<uint64_t> counter{1};
    return counter.fetch_add(1);
}

class ThreadLocalCollector : public MetricsCollector {
    const uint64_t id_ = next_collector_id();
    std::mutex list_lock_;
    std::vector<std::unique_ptr<ThreadState>> all_states_;

    ThreadState *get_my_state();

  public:
    ThreadLocalCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};

// Step 4 (double-buffering)

struct Buf {
    std::array<uint64_t, 256> buckets{};
    uint64_t count = 0;
    uint64_t sum = 0;
    uint64_t min = std::numeric_limits<uint64_t>::max();
    uint64_t max = 0;

    void clear() {
        buckets.fill(0);
        count = 0;
        sum = 0;
        min = std::numeric_limits<uint64_t>::max();
        max = 0;
    }
};

struct alignas(64) ThreadBuffers {
    std::atomic<int> inside{-1}; // -1 = нигде, 0 = буфер 0, 1 = буфер 1
    Buf buf[2];
};

class DoubleBufferedMetricsCollector : public MetricsCollector {
  protected:
    const uint64_t id_ = next_collector_id();
    std::vector<std::unique_ptr<ThreadBuffers>> all_states_;

    std::atomic<int> active_{0};
    std::mutex snap_lock_;

    std::array<uint64_t, 256> global_buckets_{};
    uint64_t global_count_{0};
    uint64_t global_sum_{0};
    uint64_t global_min_{std::numeric_limits<uint64_t>::max()};
    uint64_t global_max_{0};

    ThreadBuffers *get_my_buffers();

  public:
    DoubleBufferedMetricsCollector() = default;
    void record(uint64_t value) override;
    Snapshot snapshot() override;
};

class ExperimentalDoubleBufferedMetricsCollector : public DoubleBufferedMetricsCollector {
  public:
    ExperimentalDoubleBufferedMetricsCollector() = default;
    void record(uint64_t value) override;
    // Snapshot snapshot();
};
