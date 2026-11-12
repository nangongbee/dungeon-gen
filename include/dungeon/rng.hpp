#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace dungeon {

class Rng {
public:
    explicit Rng(uint32_t seed);

    uint32_t seed() const { return seed_; }

    int uniform(int lo, int hiExclusive);  // [lo, hi)

    double unit();  // [0, 1]

    bool chance(double probability);

    template <class T>
    void shuffle(std::vector<T>& values) {
        for (int i = static_cast<int>(values.size()) - 1; i > 0; --i) {
            const int j = uniform(0, i + 1);
            std::swap(values[static_cast<size_t>(i)], values[static_cast<size_t>(j)]);
        }
    }

private:
    uint32_t seed_ = 0;
    std::mt19937 gen_;
};

}  // namespace dungeon
