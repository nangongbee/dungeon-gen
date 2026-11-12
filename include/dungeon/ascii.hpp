#pragma once

#include "dungeon/types.hpp"

#include <string>

namespace dungeon {

std::string toAscii(const Dungeon& d);
std::string toJson(const Dungeon& d);

}  // namespace dungeon
