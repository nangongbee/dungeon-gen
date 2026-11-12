#include "dungeon/query.hpp"

#include <algorithm>
#include <queue>

namespace dungeon {
namespace {

const Room* roomPtr(const Dungeon& d, int id) {
    if (id <= 0 || id >= static_cast<int>(d.rooms.size())) {
        return nullptr;
    }
    return &d.rooms[static_cast<size_t>(id)];
}

}  // namespace

int shortestHops(const Dungeon& d, int from, int to) {
    if (from == to) {
        return 0;
    }
    if (!roomPtr(d, from) || !roomPtr(d, to)) {
        return -1;
    }
    std::vector<int> dist(d.rooms.size(), -1);
    std::queue<int> q;
    dist[static_cast<size_t>(from)] = 0;
    q.push(from);
    while (!q.empty()) {
        const int cur = q.front();
        q.pop();
        for (int nb : d.rooms[static_cast<size_t>(cur)].neighbors) {
            if (nb <= 0 || nb >= static_cast<int>(d.rooms.size()) || dist[static_cast<size_t>(nb)] >= 0) {
                continue;
            }
            dist[static_cast<size_t>(nb)] = dist[static_cast<size_t>(cur)] + 1;
            if (nb == to) {
                return dist[static_cast<size_t>(nb)];
            }
            q.push(nb);
        }
    }
    return -1;
}

std::vector<int> reachable(const Dungeon& d, int from, const std::vector<char>& blocked) {
    std::vector<int> out;
    if (!roomPtr(d, from)) {
        return out;
    }
    std::vector<char> seen(d.rooms.size(), 0);
    std::queue<int> q;
    seen[static_cast<size_t>(from)] = 1;
    q.push(from);
    while (!q.empty()) {
        const int cur = q.front();
        q.pop();
        out.push_back(cur);
        for (int nb : d.rooms[static_cast<size_t>(cur)].neighbors) {
            if (nb <= 0 || nb >= static_cast<int>(d.rooms.size()) || seen[static_cast<size_t>(nb)]) {
                continue;
            }
            if (nb < static_cast<int>(blocked.size()) && blocked[static_cast<size_t>(nb)]) {
                continue;
            }
            seen[static_cast<size_t>(nb)] = 1;
            q.push(nb);
        }
    }
    return out;
}

bool isFullyConnected(const Dungeon& d) {
    const int n = roomCount(d);
    if (n <= 0) {
        return false;
    }
    auto seen = reachable(d, 1, {});
    return static_cast<int>(seen.size()) == n;
}

bool isSolvable(const Dungeon& d) {
    const int n = roomCount(d);
    if (n <= 0 || d.lastMainId <= 0) {
        return false;
    }

    std::vector<char> haveKey(d.rooms.size(), 0);
    bool progress = true;
    std::vector<int> lastReach;
    while (progress) {
        progress = false;
        std::vector<char> blocked(d.rooms.size(), 0);
        for (int i = 1; i <= n; ++i) {
            if (d.rooms[static_cast<size_t>(i)].event == EventKind::Door && !haveKey[static_cast<size_t>(i)]) {
                blocked[static_cast<size_t>(i)] = 1;
            }
        }
        lastReach = reachable(d, 1, blocked);
        for (int id : lastReach) {
            const int door = d.rooms[static_cast<size_t>(id)].keyOpens;
            if (door > 0 && !haveKey[static_cast<size_t>(door)]) {
                haveKey[static_cast<size_t>(door)] = 1;
                progress = true;
            }
        }
    }

    // 拿到钥匙之后门房本身也可以进
    std::vector<char> blocked(d.rooms.size(), 0);
    for (int i = 1; i <= n; ++i) {
        if (d.rooms[static_cast<size_t>(i)].event == EventKind::Door && !haveKey[static_cast<size_t>(i)]) {
            blocked[static_cast<size_t>(i)] = 1;
        }
    }
    auto reach = reachable(d, 1, blocked);
    return std::find(reach.begin(), reach.end(), d.lastMainId) != reach.end();
}

}  // namespace dungeon
