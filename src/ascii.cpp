#include "dungeon/ascii.hpp"

#include <algorithm>
#include <sstream>

namespace dungeon {

std::string toAscii(const Dungeon& d) {
    if (d.rooms.size() <= 1) {
        return "(empty dungeon)\n";
    }

    int minX = d.rooms[1].pos.x, maxX = minX;
    int minY = d.rooms[1].pos.y, maxY = minY;
    for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
        minX = std::min(minX, d.rooms[static_cast<size_t>(i)].pos.x);
        maxX = std::max(maxX, d.rooms[static_cast<size_t>(i)].pos.x);
        minY = std::min(minY, d.rooms[static_cast<size_t>(i)].pos.y);
        maxY = std::max(maxY, d.rooms[static_cast<size_t>(i)].pos.y);
    }
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    const int canvasW = width * 4 + 1;
    const int canvasH = height * 3 + 1;
    std::vector<std::string> canvas(static_cast<size_t>(canvasH), std::string(static_cast<size_t>(canvasW), ' '));

    auto draw = [&](int x, int y, char c) {
        if (y >= 0 && y < canvasH && x >= 0 && x < canvasW) {
            canvas[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
        }
    };

    for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
        const auto& room = d.rooms[static_cast<size_t>(i)];
        const int x = room.pos.x - minX;
        const int y = room.pos.y - minY;
        const int cy = (height - 1 - y) * 3 + 1;
        const int cx = x * 4 + 2;

        char symbol = room.branch == 0 ? 'M' : 'o';
        if (room.id == 1) {
            symbol = 'S';
        } else if (room.id == d.lastMainId) {
            symbol = 'B';
        } else if (room.event == EventKind::Door) {
            symbol = 'D';
        } else if (room.keyOpens > 0) {
            symbol = 'k';
        } else if (room.event == EventKind::SuperChest) {
            symbol = 'C';
        } else if (room.event == EventKind::MiddleChest || room.event == EventKind::SmallChest) {
            symbol = 'c';
        } else if (room.enemy == EnemyKind::Elite) {
            symbol = 'E';
        } else if (room.enemy == EnemyKind::Normal) {
            symbol = 'm';
        }

        draw(cx, cy, symbol);
        draw(cx - 1, cy, '[');
        draw(cx + 1, cy, ']');

        for (int nb : room.neighbors) {
            if (nb < room.id) {
                continue;
            }
            const auto& other = d.rooms[static_cast<size_t>(nb)];
            const int nx = other.pos.x - minX;
            const int ny = other.pos.y - minY;
            if (nx == x) {
                if (ny > y) {
                    draw(cx, cy - 1, '|');
                    draw(cx, cy - 2, '|');
                } else {
                    draw(cx, cy + 1, '|');
                    draw(cx, cy + 2, '|');
                }
            } else if (ny == y) {
                if (nx > x) {
                    draw(cx + 2, cy, '-');
                } else {
                    draw(cx - 2, cy, '-');
                }
            }
        }
    }

    std::ostringstream o;
    o << "Seed: " << d.seed << "  Rooms: " << roomCount(d) << "  Main end: " << d.lastMainId << "\n";
    o << "Legend: S start  B boss  M main  o branch  D door  k key  C/c chest  E elite  m monster\n";
    for (const auto& line : canvas) {
        o << line << "\n";
    }
    return o.str();
}

std::string toJson(const Dungeon& d) {
    std::ostringstream o;
    o << "{\n  \"seed\": " << d.seed << ",\n  \"lastMainId\": " << d.lastMainId << ",\n  \"rooms\": [\n";
    for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
        const auto& r = d.rooms[static_cast<size_t>(i)];
        o << "    {\"id\": " << r.id << ", \"x\": " << r.pos.x << ", \"y\": " << r.pos.y
          << ", \"branch\": " << r.branch << ", \"event\": \"" << eventName(r.event)
          << "\", \"enemy\": \"" << enemyName(r.enemy) << "\", \"keyOpens\": " << r.keyOpens
          << ", \"neighbors\": [";
        for (size_t n = 0; n < r.neighbors.size(); ++n) {
            if (n) {
                o << ", ";
            }
            o << r.neighbors[n];
        }
        o << "]}";
        if (i + 1 < static_cast<int>(d.rooms.size())) {
            o << ",";
        }
        o << "\n";
    }
    o << "  ]\n}\n";
    return o.str();
}

}  // namespace dungeon
