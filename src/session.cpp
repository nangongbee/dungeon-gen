#include "dungeon/session.hpp"

#include <algorithm>

namespace dungeon {

Session::Session(Dungeon dungeon) : dungeon_(std::move(dungeon)) {
    keys_.assign(dungeon_.rooms.size(), 0);
    player_ = dungeon_.rooms.size() > 1 ? 1 : 0;
    if (player_ > 0 && player_ < static_cast<int>(dungeon_.rooms.size())) {
        // 出生点偶尔也会掉钥匙
        const int door = dungeon_.rooms[static_cast<size_t>(player_)].keyOpens;
        if (door > 0 && door < static_cast<int>(keys_.size())) {
            keys_[static_cast<size_t>(door)] = 1;
        }
    }
}

bool Session::hasKeyFor(int doorRoomId) const {
    return doorRoomId > 0 && doorRoomId < static_cast<int>(keys_.size()) &&
           keys_[static_cast<size_t>(doorRoomId)];
}

bool Session::canEnter(int roomId) const {
    if (roomId <= 0 || roomId >= static_cast<int>(dungeon_.rooms.size())) {
        return false;
    }
    const auto& room = dungeon_.rooms[static_cast<size_t>(roomId)];
    if (room.event == EventKind::Door && !hasKeyFor(roomId)) {
        // 没钥匙不能进门房
        return false;
    }
    return true;
}

bool Session::tryMove(int roomId) {
    if (won_ || player_ <= 0 || player_ >= static_cast<int>(dungeon_.rooms.size())) {
        return false;
    }
    const auto& cur = dungeon_.rooms[static_cast<size_t>(player_)];
    if (std::find(cur.neighbors.begin(), cur.neighbors.end(), roomId) == cur.neighbors.end()) {
        return false;
    }
    if (!canEnter(roomId)) {
        return false;
    }
    player_ = roomId;
    ++steps_;
    const int door = dungeon_.rooms[static_cast<size_t>(player_)].keyOpens;
    if (door > 0 && door < static_cast<int>(keys_.size())) {
        keys_[static_cast<size_t>(door)] = 1;
    }
    if (player_ == dungeon_.lastMainId) {
        won_ = true;
    }
    return true;
}

std::vector<int> Session::legalMoves() const {
    std::vector<int> out;
    if (player_ <= 0 || player_ >= static_cast<int>(dungeon_.rooms.size())) {
        return out;
    }
    const auto& cur = dungeon_.rooms[static_cast<size_t>(player_)];
    for (int nb : cur.neighbors) {
        if (canEnter(nb)) {
            out.push_back(nb);
        }
    }
    return out;
}

}  // namespace dungeon
