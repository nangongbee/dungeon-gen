#pragma once

#include "dungeon/config.hpp"
#include "dungeon/types.hpp"

#include <optional>
#include <string>

namespace dungeon {

enum class GenError {
    Ok = 0,
    MainPathStuck,
    FillFailed,
};

struct GenResult {
    std::optional<Dungeon> dungeon;
    GenError error = GenError::Ok;
    int retries = 0;
};

// 同一 seed 可复现；失败会换偏移再试几次
GenResult generate(const Config& cfg, uint32_t seed);

std::string errorMessage(GenError err);

}  // namespace dungeon
