#pragma once

#include "dungeon/types.hpp"

#include <vector>

namespace dungeon {

class Session {
public:
    explicit Session(Dungeon dungeon);

    bool tryMove(int roomId);
    std::vector<int> legalMoves() const;
    bool hasKeyFor(int doorRoomId) const;

    int player() const { return player_; }
    int steps() const { return steps_; }
    bool won() const { return won_; }
    const Dungeon& dungeon() const { return dungeon_; }

private:
    bool canEnter(int roomId) const;

    Dungeon dungeon_;
    int player_ = 1;
    int steps_ = 0;
    bool won_ = false;
    std::vector<char> keys_;  // index by door room id
};

}  // namespace dungeon
