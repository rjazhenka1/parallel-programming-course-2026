#include "metrics_collector.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

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
    count_++;
    sum_ += value;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
}

Snapshot STMetricsCollector::snapshot() {
    uint64_t count = count_;

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
    count_++;
    sum_ += value;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
}

Snapshot MutexMetricsCollector::snapshot() {
    std::unique_lock<std::mutex> lock{mutex_};

    uint64_t count = count_;

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

// Step 2

void StripedMutexMetricsCollector::record(uint64_t value) {
    std::size_t bucket_index = std::min(value / 4, uint64_t{255}); // no mutex needed here
    std::size_t lock_index = bucket_index % 16;

    std::unique_lock<std::mutex> lock{mutexes_[lock_index]};
    buckets_[bucket_index]++;
    lock.unlock();

    count_.fetch_add(1, std::memory_order::relaxed);

    uint64_t expected_sum = sum_.load(std::memory_order::relaxed);
    uint64_t new_sum;
    do {
        new_sum = expected_sum + value;
    } while (!sum_.compare_exchange_weak(expected_sum, new_sum, std::memory_order::relaxed));

    uint64_t expected_min = min_.load(std::memory_order::relaxed);
    uint64_t new_min;
    do {
        new_min = std::min(expected_min, value);
    } while (!min_.compare_exchange_weak(expected_min, new_min, std::memory_order::relaxed));

    uint64_t expected_max = max_.load(std::memory_order::relaxed);
    uint64_t new_max;
    do {
        new_max = std::max(expected_max, value);
    } while (!max_.compare_exchange_weak(expected_max, new_max, std::memory_order::relaxed));
}

Snapshot StripedMutexMetricsCollector::snapshot() {
    std::array<uint64_t, 256> buckets;
    uint64_t count;
    uint64_t sum;
    uint64_t min;
    uint64_t max;
    uint64_t p50;
    uint64_t p99;

    for (std::size_t g = 0; g < 16; g++) {
        std::unique_lock<std::mutex> lock{mutexes_[g]};
        for (std::size_t i = g; i < 256; i += 16) {
            buckets[i] = buckets_[i];
        }
    }

    count = count_.load(std::memory_order::relaxed);
    sum = sum_.load(std::memory_order::relaxed);
    min = min_.load(std::memory_order::relaxed);
    max = max_.load(std::memory_order::relaxed);

    p50 = find_percentile(buckets, count, 0.5);
    p99 = find_percentile(buckets, count, 0.99);

    Snapshot snapshot{
        .buckets = buckets,
        .count = count,
        .sum = sum,
        .min = min,
        .max = max,
        .p50 = p50,
        .p99 = p99,
    };

    return snapshot;
}

// Step 3

namespace {

inline void relaxed_add(std::atomic<uint64_t> &c, uint64_t delta) {
    c.store(c.load(std::memory_order::relaxed) + delta, std::memory_order::relaxed);
}

} // namespace

ThreadState *ThreadLocalCollector::get_my_state() {
    struct TLSSlot {
        uint64_t id = 0;
        ThreadState *state = nullptr;
    };
    static thread_local TLSSlot slot;
    if (slot.id != id_) {
        auto s = std::make_unique<ThreadState>();
        ThreadState *raw = s.get();
        {
            std::unique_lock<std::mutex> g(list_lock_);
            all_states_.push_back(std::move(s));
        }
        slot.id = id_;
        slot.state = raw;
    }
    return slot.state;
}

void ThreadLocalCollector::record(uint64_t value) {
    ThreadState *s = get_my_state();
    uint64_t b = std::min(value / 4, uint64_t{255});

    relaxed_add(s->buckets[b], 1);
    relaxed_add(s->count, 1);
    relaxed_add(s->sum, value);

    if (value < s->min.load(std::memory_order::relaxed))
        s->min.store(value, std::memory_order::relaxed);
    if (value > s->max.load(std::memory_order::relaxed))
        s->max.store(value, std::memory_order::relaxed);
}

Snapshot ThreadLocalCollector::snapshot() {
    std::array<uint64_t, 256> buckets{};
    uint64_t count = 0;
    uint64_t sum = 0;
    uint64_t min = std::numeric_limits<uint64_t>::max();
    uint64_t max = 0;

    std::vector<ThreadState *> states_copy;
    {
        std::unique_lock<std::mutex> lock{list_lock_};
        states_copy.reserve(all_states_.size());
        for (const auto &state : all_states_) {
            states_copy.push_back(state.get());
        }
    }

    for (const auto &state : states_copy) {
        for (std::size_t i = 0; i < 256; i++) {
            buckets[i] += state->buckets[i].load(std::memory_order::relaxed);
        }

        count += state->count.load(std::memory_order::relaxed);
        sum += state->sum.load(std::memory_order::relaxed);
        min = std::min(min, state->min.load(std::memory_order::relaxed));
        max = std::max(max, state->max.load(std::memory_order::relaxed));
    }

    const auto p50 = find_percentile(buckets, count, 0.5);
    const auto p99 = find_percentile(buckets, count, 0.99);

    return Snapshot{
        .buckets = buckets,
        .count = count,
        .sum = sum,
        .min = min,
        .max = max,
        .p50 = p50,
        .p99 = p99,
    };
}

// Step 4

ThreadBuffers *DoubleBufferedMetricsCollector::get_my_buffers() {
    struct TLSSlot {
        uint64_t id = 0;
        ThreadBuffers *buffers = nullptr;
    };
    static thread_local TLSSlot slot;
    if (slot.id != id_) {
        auto s = std::make_unique<ThreadBuffers>();
        ThreadBuffers *raw = s.get();
        {
            std::unique_lock<std::mutex> g(snap_lock_);
            all_states_.push_back(std::move(s));
        }
        slot.id = id_;
        slot.buffers = raw;
    }
    return slot.buffers;
}

void DoubleBufferedMetricsCollector::record(uint64_t value) {
    auto &my = *get_my_buffers();

    uint64_t b;
    while (true) {
        b = active_.load(std::memory_order::seq_cst);
        my.inside.store(b);
        if (active_.load(std::memory_order::seq_cst) == b) {
            break;
        }
        my.inside.store(-1, std::memory_order::release);
    }

    auto &buf = my.buf[b];
    std::size_t bucket_index = std::min(value / 4, uint64_t{255});

    buf.buckets[bucket_index]++;
    buf.count++;
    buf.sum += value;
    buf.min = std::min(buf.min, value);
    buf.max = std::max(buf.max, value);

    my.inside.store(-1, std::memory_order::release);
}

Snapshot DoubleBufferedMetricsCollector::snapshot() {
    std::unique_lock<std::mutex> lock{snap_lock_};

    uint64_t old = active_.load(std::memory_order::seq_cst);
    active_.store(1 - old, std::memory_order::seq_cst);

    for (auto &state : all_states_) {
        while (state->inside.load(std::memory_order::seq_cst) == old) {
            std::this_thread::yield();
        }

        for (std::size_t i = 0; i < 256; i++) {
            global_buckets_[i] += state->buf[old].buckets[i];
        }
        global_count_ += state->buf[old].count;
        global_sum_ += state->buf[old].sum;
        global_min_ = std::min(global_min_, state->buf[old].min);
        global_max_ = std::max(global_max_, state->buf[old].max);

        state->buf[old].buckets.fill(0);
        state->buf[old].count = 0;
        state->buf[old].sum = 0;
        state->buf[old].min = std::numeric_limits<uint64_t>::max();
        state->buf[old].max = 0;
    }

    const auto p50 = find_percentile(global_buckets_, global_count_, 0.5);
    const auto p99 = find_percentile(global_buckets_, global_count_, 0.99);

    return Snapshot{
        .buckets = global_buckets_,
        .count = global_count_,
        .sum = global_sum_,
        .min = global_min_,
        .max = global_max_,
        .p50 = p50,
        .p99 = p99,
    };
}

void ExperimentalDoubleBufferedMetricsCollector::record(uint64_t value) {
    auto &my = *get_my_buffers();

    uint64_t b = active_.load(std::memory_order::seq_cst);
    my.inside.store(b);

    auto &buf = my.buf[b];
    std::size_t bucket_index = std::min(value / 4, uint64_t{255});

    buf.buckets[bucket_index]++;
    buf.count++;
    buf.sum += value;
    buf.min = std::min(buf.min, value);
    buf.max = std::max(buf.max, value);

    my.inside.store(-1, std::memory_order::release);
}
