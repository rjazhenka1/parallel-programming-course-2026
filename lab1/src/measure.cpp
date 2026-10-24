#include "measure.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <numeric>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
double run(MetricsCollector &collector, const std::vector<uint64_t> &values, const std::size_t thread_count,
           const std::chrono::milliseconds &run_time) {
    std::latch start{1};
    std::atomic<bool> stop{false};
    std::vector<uint64_t> counts(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t i = 0; i < thread_count; i++) {
        threads.emplace_back([&collector, &values, &start, &stop, &counts, i]() {
            std::size_t local_count = 0;
            std::size_t vi = i * 1000;
            start.wait();
            while (!stop) {
                collector.record(values[vi]);
                local_count++;
                vi++;
                if (vi == values.size()) {
                    vi = 0;
                }
            }
            counts[i] = local_count;
        });
    }

    const auto t0 = std::chrono::steady_clock::now();
    start.count_down();
    std::this_thread::sleep_for(run_time);
    stop.store(true, std::memory_order::relaxed);
    const auto t1 = std::chrono::steady_clock::now();

    for (auto &t : threads) {
        t.join();
    }

    const double total_ops = std::accumulate(counts.begin(), counts.end(), 0.0);
    const double elapsed_seconds = std::chrono::duration<double>(t1 - t0).count();
    return total_ops / elapsed_seconds;
}

template <typename T> T median(const std::vector<T> &values) {
    std::vector<T> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();

    if (n % 2 == 0) {
        return (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
    }

    return sorted[n / 2];
}

} // namespace

MeasureResult measure_point(MetricsCollector &collector, const std::vector<uint64_t> &values,
                            const std::size_t thread_count) {
    run(collector, values, thread_count, 5s);

    const std::size_t runs = 5;
    std::vector<double> results;
    results.reserve(runs);

    for (std::size_t i = 0; i < runs; i++) {
        results.push_back(run(collector, values, thread_count, 5s));
    }

    return MeasureResult{
        .collector_snapshot = collector.snapshot(),
        .ops = median(results),
    };
}

ConsistencyResult consistency_check(MetricsCollector &collector, const std::vector<uint64_t> &values,
                                    const std::size_t thread_count) {
    std::atomic<bool> stop{false};
    std::vector<uint64_t> true_call_counts(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t i = 0; i < thread_count; i++) {
        threads.emplace_back([&collector, &values, &stop, &true_call_counts, i]() {
            std::size_t local_count = 0;
            std::size_t vi = i * 1000;
            while (!stop) {
                collector.record(values[vi]);
                local_count++;
                vi++;
                if (vi == values.size()) {
                    vi = 0;
                }
            }
            true_call_counts[i] = local_count;
        });
    }

    ConsistencyResult result;
    constexpr uint64_t consistency_check_count = 10000;
    for (std::size_t i = 0; i < consistency_check_count; i++) {
        const auto &snapshot = collector.snapshot();

        const auto &sum_buckets = std::accumulate(snapshot.buckets.begin(), snapshot.buckets.end(), uint64_t{0});
        const auto &count = snapshot.count;

        if (sum_buckets < count) {
            result.check_less_count++;
        } else if (sum_buckets > count) {
            result.check_greater_count++;
        } else {
            result.check_equal_count++;
        }
        result.total_check_count++;
    }

    stop.store(true, std::memory_order::relaxed);

    for (auto &t : threads) {
        t.join();
    }

    result.collector_snapshot = collector.snapshot();
    result.true_call_count = std::accumulate(true_call_counts.begin(), true_call_counts.end(), uint64_t{0});

    return result;
}
