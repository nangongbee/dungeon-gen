#include "dungeon/ascii.hpp"
#include "dungeon/config.hpp"
#include "dungeon/generate.hpp"
#include "dungeon/query.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    dungeon::Config cfg = dungeon::defaultConfig();
    uint32_t seed = static_cast<uint32_t>(std::time(nullptr));
    bool dumpJson = false;
    std::string configPath;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--seed" || arg == "-s") {
            seed = static_cast<uint32_t>(std::stoul(need("--seed")));
        } else if (arg == "--config" || arg == "-c") {
            configPath = need("--config");
        } else if (arg == "--json") {
            dumpJson = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "dungeon-cli [--seed N] [--config path] [--json]\n"
                << "  Generates a dungeon and prints an ASCII map.\n";
            return 0;
        } else {
            // 直接跟一个数字也当成 seed
            if (!arg.empty() && std::isdigit(static_cast<unsigned char>(arg[0]))) {
                seed = static_cast<uint32_t>(std::stoul(arg));
            } else {
                std::cerr << "unknown argument: " << arg << "\n";
                return 2;
            }
        }
    }

    if (!configPath.empty()) {
        try {
            cfg = dungeon::loadConfig(configPath);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    }

    const auto result = dungeon::generate(cfg, seed);
    if (!result.dungeon) {
        std::cerr << "generation failed: " << dungeon::errorMessage(result.error) << "\n";
        return 1;
    }
    const auto& d = *result.dungeon;
    if (dumpJson) {
        std::cout << dungeon::toJson(d);
    } else {
        std::cout << dungeon::toAscii(d);
        std::cout << "connected=" << (dungeon::isFullyConnected(d) ? "yes" : "no")
                  << "  solvable=" << (dungeon::isSolvable(d) ? "yes" : "no")
                  << "  start->boss hops=" << dungeon::shortestHops(d, 1, d.lastMainId) << "\n";
    }
    return 0;
}
