#pragma once

#include <string>

namespace dungeon {

struct Range {
    int min = 0;
    int max = 0;
};

struct Config {
    Range mainRooms{10, 12};
    Range totalRooms{20, 25};
    Range subPaths{3, 4};
    Range monsterNums{5, 8};
    int doors = 2;
    int superChests = 1;
    int middleChests = 2;
    int smallChests = 0;
    bool hasBoss = true;
    double shortcutRatio = 0.6;  // 支线接回后，主线最短路不得短于这个比例
    int sizeRatioLimit = 4;      // 宽/高或高/宽超过这个值的格子直接丢掉
    double eliteRate = 0.0;
    int maxRetries = 5;
};

Config defaultConfig();
Config loadConfig(const std::string& path);
std::string saveConfig(const Config& cfg);

}  // namespace dungeon
