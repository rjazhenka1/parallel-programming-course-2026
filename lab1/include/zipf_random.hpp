#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

template <typename T> class ZipfRandom {
    std::mt19937_64 engine_;
    std::discrete_distribution<T> dist_;

  public:
    ZipfRandom(uint64_t seed) {
        std::array<double, 1024> weights;

        weights[0] = 0;
        for (std::size_t i = 1; i < weights.size(); i++) {
            weights[i] = 1.0 / std::pow(i, 1.15);
        }

        dist_ = std::discrete_distribution<T>{weights.begin(), weights.end()};
        engine_ = std::mt19937_64{seed};
    }

    std::vector<T> generate(std::size_t count) {
        std::vector<T> result;
        result.reserve(count);

        for (std::size_t i = 0; i < count; i++) {
            result.emplace_back(static_cast<T>(dist_(engine_)));
        }

        return result;
    }
};
