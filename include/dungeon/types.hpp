#pragma once

#include <cstdint>
#include <vector>

namespace dungeon {

struct Pos {
    int x = 0;
    int y = 0;

    friend bool operator==(const Pos&, const Pos&) = default;
    friend auto operator<=>(const Pos&, const Pos&) = default;
};

inline Pos operator+(Pos a, Pos b) { return {a.x + b.x, a.y + b.y}; }

enum class EventKind : uint8_t {
    None = 0,
    Door,
    SuperChest,
    MiddleChest,
    SmallChest,
};

enum class EnemyKind : uint8_t {
    None = 0,
    Normal,
    Elite,
    Boss,
};

struct Room {
    int id = 0;
    Pos pos{};
    int branch = 0;  // 0 主线, >0 支线编号（从 1 起）
    std::vector<int> neighbors;
    EventKind event = EventKind::None;
    EnemyKind enemy = EnemyKind::None;
    int keyOpens = 0;  // 这间房掉的钥匙能开哪扇门；0 表示没有
};

struct Dungeon {
    uint32_t seed = 0;
    int lastMainId = 0;
    std::vector<Room> rooms;  // [0] 空着，id 就是下标
};

inline int roomCount(const Dungeon& d) {
    return d.rooms.empty() ? 0 : static_cast<int>(d.rooms.size()) - 1;
}

inline bool isMainRoom(const Dungeon& d, int id) { return id > 0 && id <= d.lastMainId; }

inline const char* eventName(EventKind k) {
    switch (k) {
        case EventKind::Door: return "door";
        case EventKind::SuperChest: return "super_chest";
        case EventKind::MiddleChest: return "middle_chest";
        case EventKind::SmallChest: return "small_chest";
        default: return "none";
    }
}

inline const char* enemyName(EnemyKind k) {
    switch (k) {
        case EnemyKind::Normal: return "normal";
        case EnemyKind::Elite: return "elite";
        case EnemyKind::Boss: return "boss";
        default: return "none";
    }
}

}  // namespace dungeon
