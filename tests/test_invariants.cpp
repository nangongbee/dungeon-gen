#include "dungeon/ascii.hpp"
#include "dungeon/config.hpp"
#include "dungeon/generate.hpp"
#include "dungeon/query.hpp"
#include "dungeon/session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool cond, const std::string& msg) {
    if (cond) {
        ++g_passed;
    } else {
        ++g_failed;
        std::cerr << "FAIL: " << msg << "\n";
    }
}

dungeon::Dungeon mustGenerate(const dungeon::Config& cfg, uint32_t seed, const char* tag) {
    const auto r = dungeon::generate(cfg, seed);
    check(r.dungeon.has_value(), std::string(tag) + " generated");
    if (!r.dungeon) {
        std::cerr << "  error=" << dungeon::errorMessage(r.error) << " seed=" << seed << "\n";
        return {};
    }
    return *r.dungeon;
}

}  // namespace

int main() {
    const dungeon::Config cfg = dungeon::defaultConfig();

    {
        const auto a = mustGenerate(cfg, 12345, "seed 12345 a");
        const auto b = mustGenerate(cfg, 12345, "seed 12345 b");
        check(dungeon::toJson(a) == dungeon::toJson(b), "same seed is byte-identical");
        check(dungeon::toJson(a) != dungeon::toJson(mustGenerate(cfg, 12346, "seed 12346")),
              "different seeds differ");
    }

    constexpr int kTrials = 80;
    int connected = 0;
    int solvable = 0;
    int shortcutOk = 0;
    int keysMatchDoors = 0;
    for (int i = 0; i < kTrials; ++i) {
        const uint32_t seed = 2000u + static_cast<uint32_t>(i) * 97u;
        const auto d = mustGenerate(cfg, seed, "trial");
        if (d.rooms.size() <= 1) {
            continue;
        }
        if (dungeon::isFullyConnected(d)) {
            ++connected;
        }
        if (dungeon::isSolvable(d)) {
            ++solvable;
        }
        const int hops = dungeon::shortestHops(d, 1, d.lastMainId);
        const int limit = static_cast<int>(std::ceil(cfg.shortcutRatio * d.lastMainId));
        if (hops >= 0 && hops + 1 >= limit) {
            ++shortcutOk;
        }

        int doors = 0;
        int keys = 0;
        bool dupNeighbor = false;
        bool badEdge = false;
        for (int id = 1; id < static_cast<int>(d.rooms.size()); ++id) {
            const auto& room = d.rooms[static_cast<size_t>(id)];
            if (room.event == dungeon::EventKind::Door) {
                ++doors;
            }
            if (room.keyOpens > 0) {
                ++keys;
            }
            std::vector<int> seenNb;
            for (int nb : room.neighbors) {
                if (std::find(seenNb.begin(), seenNb.end(), nb) != seenNb.end()) {
                    dupNeighbor = true;
                }
                seenNb.push_back(nb);
                if (nb <= 0 || nb >= static_cast<int>(d.rooms.size())) {
                    badEdge = true;
                    continue;
                }
                const auto& other = d.rooms[static_cast<size_t>(nb)];
                if (std::abs(other.pos.x - room.pos.x) + std::abs(other.pos.y - room.pos.y) != 1) {
                    badEdge = true;
                }
            }
        }
        check(!dupNeighbor, "no duplicate neighbors seed=" + std::to_string(seed));
        check(!badEdge, "edges are 4-adjacent seed=" + std::to_string(seed));
        if (doors == keys) {
            ++keysMatchDoors;
        }

        dungeon::Session session(d);
        check(session.player() == 1, "session starts at room 1");
        for (int nb : d.rooms[1].neighbors) {
            if (d.rooms[static_cast<size_t>(nb)].event == dungeon::EventKind::Door &&
                !session.hasKeyFor(nb)) {
                check(!session.tryMove(nb), "cannot walk into a locked door");
            }
        }
    }

    check(connected == kTrials, "all maps connected (" + std::to_string(connected) + ")");
    check(solvable == kTrials, "all maps solvable (" + std::to_string(solvable) + ")");
    check(shortcutOk == kTrials, "all maps respect shortcut ratio (" + std::to_string(shortcutOk) + ")");
    check(keysMatchDoors == kTrials, "each door has one key (" + std::to_string(keysMatchDoors) + ")");

    std::cout << "passed=" << g_passed << " failed=" << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
