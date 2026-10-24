#include "measure.hpp"
#include "metrics_collector.hpp"
#include "zipf_random.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <thread>

constexpr uint64_t seed = 67676767;
constexpr uint64_t required_values = (1 << 20);
const std::size_t nproc = std::thread::hardware_concurrency();

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <step>\n";
        return 1;
    }

    const std::string_view step_arg{argv[1]};
    int step = -1;
    const auto result = std::from_chars(step_arg.data(), step_arg.data() + step_arg.size(), step);
    if (result.ec != std::errc{} || result.ptr != step_arg.data() + step_arg.size()) {
        std::cerr << "invalid step: " << step_arg << "\n";
        return 1;
    }

    if (nproc == 0) {
        std::cerr << "could not get number of threads" << std::endl;
        return 2;
    }

    switch (step) {
    case 0: {
        std::cout << "running measurements for step 0:" << std::endl;
        std::cout << std::endl;
        std::cout << "\tthread count = 1" << std::endl;

        ZipfRandom<uint64_t> zipf_random(seed);
        const auto values = zipf_random.generate(required_values);
        STMetricsCollector collector;

        const auto result = measure_point(collector, values, 1);
        std::cout << "\tops/sec = " << std::fixed << std::setprecision(2) << result.ops << std::endl;

        break;
    }
    case 1: {
        std::cout << "running measurements for step 1 (real measurement w/ mutex):" << std::endl;
        for (std::size_t thread_count = 1; thread_count <= nproc; thread_count++) {
            std::cout << std::endl;
            std::cout << "\tthread count = " << thread_count << std::endl;

            ZipfRandom<uint64_t> zipf_random(seed);
            const auto values = zipf_random.generate(required_values);
            MutexMetricsCollector collector;

            const auto result = measure_point(collector, values, thread_count);
            std::cout << "\tops/sec = " << std::fixed << std::setprecision(2) << result.ops << std::endl;
        }

        std::cout << std::endl << "running measurements for step 1 (empty code w/ mutex):" << std::endl;
        for (std::size_t thread_count = 1; thread_count <= nproc; thread_count++) {
            std::cout << std::endl;
            std::cout << "\tthread count = " << thread_count << std::endl;

            ZipfRandom<uint64_t> zipf_random(seed);
            const auto values = zipf_random.generate(required_values);
            EmptyMutexMetricsCollector collector;

            const auto result = measure_point(collector, values, thread_count);
            std::cout << "\tops/sec = " << std::fixed << std::setprecision(2) << result.ops << std::endl;
        }

        break;
    }
    case 2: {
        {
            std::cout << "running measurements for step 2:" << std::endl;
            for (std::size_t thread_count = 1; thread_count <= nproc; thread_count++) {
                std::cout << std::endl;
                std::cout << "\tthread count = " << thread_count << std::endl;

                ZipfRandom<uint64_t> zipf_random(seed);
                const auto values = zipf_random.generate(required_values);
                StripedMutexMetricsCollector collector;

                const auto result = measure_point(collector, values, thread_count);
                std::cout << "\tops/sec = " << std::fixed << std::setprecision(2) << result.ops << std::endl;
            }
        }
        {
            std::cout << std::endl << std::endl << "consistency check for step 2:" << std::endl;
            const uint64_t thread_count = 4;
            std::cout << std::endl;
            std::cout << "\tthread count = " << thread_count << std::endl;

            ZipfRandom<uint64_t> zipf_random(seed);
            const auto values = zipf_random.generate(required_values);
            StripedMutexMetricsCollector collector;

            const auto result = consistency_check(collector, values, thread_count);
            std::cout << "\ttotal checks = " << result.total_check_count << std::endl;
            std::cout << "\t\tsum(buckets) == count = " << result.check_equal_count << std::endl;
            std::cout << "\t\tsum(buckets) < count = " << result.check_less_count << std::endl;
            std::cout << "\t\tsum(buckets) > count = " << result.check_greater_count << std::endl;
            std::cout << "\t\trecorded count = " << result.collector_snapshot.count << std::endl;
            std::cout << "\t\ttrue call  count = " << result.true_call_count << std::endl;
            break;
        }
    }
    case 3: {
        {
            std::cout << "running measurements for step 3:" << std::endl;
            for (std::size_t thread_count = 1; thread_count <= nproc; thread_count++) {
                std::cout << std::endl;
                std::cout << "\tthread count = " << thread_count << std::endl;

                ZipfRandom<uint64_t> zipf_random(seed);
                const auto values = zipf_random.generate(required_values);
                ThreadLocalCollector collector;

                const auto result = measure_point(collector, values, thread_count);
                std::cout << "\tops/sec = " << std::fixed << std::setprecision(2) << result.ops << std::endl;
            }
        }

        {
            std::cout << std::endl << std::endl << "consistency check for step 3:" << std::endl;
            const uint64_t thread_count = 4;
            std::cout << std::endl;
            std::cout << "\tthread count = " << thread_count << std::endl;

            ZipfRandom<uint64_t> zipf_random(seed);
            const auto values = zipf_random.generate(required_values);
            ThreadLocalCollector collector;

            const auto result = consistency_check(collector, values, thread_count);
            std::cout << "\ttotal checks = " << result.total_check_count << std::endl;
            std::cout << "\t\tsum(buckets) == count = " << result.check_equal_count << std::endl;
            std::cout << "\t\tsum(buckets) < count = " << result.check_less_count << std::endl;
            std::cout << "\t\tsum(buckets) > count = " << result.check_greater_count << std::endl;
            std::cout << "\t\trecorded count = " << result.collector_snapshot.count << std::endl;
            std::cout << "\t\ttrue call  count = " << result.true_call_count << std::endl;
            break;
        }
    }
    case 4: {
        {
            std::cout << "running measurements for step 4:" << std::endl;
            for (std::size_t thread_count = 1; thread_count <= nproc; thread_count++) {
                std::cout << std::endl;
                std::cout << "\tthread count = " << thread_count << std::endl;

                ZipfRandom<uint64_t> zipf_random(seed);
                const auto values = zipf_random.generate(required_values);
                DoubleBufferedMetricsCollector collector;

                const auto result = measure_point(collector, values, thread_count);
                std::cout << "\tops/sec = " << std::fixed << std::setprecision(2) << result.ops << std::endl;
            }
        }
        {
            std::cout << std::endl << std::endl << "consistency check for step 4:" << std::endl;
            const uint64_t thread_count = 4;
            std::cout << std::endl;
            std::cout << "\tthread count = " << thread_count << std::endl;

            ZipfRandom<uint64_t> zipf_random(seed);
            const auto values = zipf_random.generate(required_values);
            DoubleBufferedMetricsCollector collector;

            const auto result = consistency_check(collector, values, thread_count);
            std::cout << "\ttotal checks = " << result.total_check_count << std::endl;
            std::cout << "\t\tsum(buckets) == count = " << result.check_equal_count << std::endl;
            std::cout << "\t\tsum(buckets) < count = " << result.check_less_count << std::endl;
            std::cout << "\t\tsum(buckets) > count = " << result.check_greater_count << std::endl;
            std::cout << "\t\trecorded count = " << result.collector_snapshot.count << std::endl;
            std::cout << "\t\ttrue call  count = " << result.true_call_count << std::endl;
        }
        {
            std::cout << std::endl << std::endl << "consistency check for step 4 (no active check):" << std::endl;
            const uint64_t thread_count = 4;
            std::cout << std::endl;
            std::cout << "\tthread count = " << thread_count << std::endl;

            ZipfRandom<uint64_t> zipf_random(seed);
            const auto values = zipf_random.generate(required_values);
            ExperimentalDoubleBufferedMetricsCollector collector;

            const auto result = consistency_check(collector, values, thread_count);
            std::cout << "\ttotal checks = " << result.total_check_count << std::endl;
            std::cout << "\t\tsum(buckets) == count = " << result.check_equal_count << std::endl;
            std::cout << "\t\tsum(buckets) < count = " << result.check_less_count << std::endl;
            std::cout << "\t\tsum(buckets) > count = " << result.check_greater_count << std::endl;
            std::cout << "\t\trecorded count = " << result.collector_snapshot.count << std::endl;
            std::cout << "\t\ttrue call  count = " << result.true_call_count << std::endl;
            break;
        }
    }
    default:
        std::cerr << "unknown step: " << step << "\n";
        break;
    }
}
