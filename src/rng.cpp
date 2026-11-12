#include "dungeon/rng.hpp"

namespace dungeon {

Rng::Rng(uint32_t seed) : seed_(seed), gen_(seed) {}

int Rng::uniform(int lo, int hiExclusive) {
    if (hiExclusive <= lo) {
        return lo;
    }
    std::uniform_int_distribution<int> dist(lo, hiExclusive - 1);
    return dist(gen_);
}

double Rng::unit() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen_);
}

bool Rng::chance(double probability) {
    if (probability <= 0.0) {
        return false;
    }
    if (probability >= 1.0) {
        return true;
    }
    return unit() <= probability;
}

}  // namespace dungeon
