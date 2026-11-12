#pragma once

#include "dungeon/types.hpp"

#include <vector>

namespace dungeon {

int shortestHops(const Dungeon& d, int from, int to);

// blocked 当墙，墙本身不计入返回值
std::vector<int> reachable(const Dungeon& d, int from, const std::vector<char>& blocked);

bool isFullyConnected(const Dungeon& d);

// 钥匙能按某种顺序捡齐、从而走到 Boss。
bool isSolvable(const Dungeon& d);

}  // namespace dungeon
