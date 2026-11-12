#include "dungeon/generate.hpp"

#include "dungeon/rng.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <queue>

namespace dungeon {
namespace {

constexpr Pos kDirs[4] = {{0, 1}, {0, -1}, {-1, 0}, {1, 0}};
// 主线至少这么多房间才考虑再放一扇门，避免开局就被锁住
constexpr int kDoorPerMainRooms = 15;

int sampleRange(Rng& rng, Range r) {
    if (r.max < r.min) {
        std::swap(r.min, r.max);
    }
    return rng.uniform(r.min, r.max + 1);
}

class Builder {
public:
    struct Branch {
        int mainRoomId = 0;
        std::vector<int> path;
        int connectedRoomId = 0;
        int depth = 0;
        int innerRoomId = 0;
        int doorRoomId = 0;
    };

    Builder(Rng& rng, int sizeRatioLimit) : rng_(rng), sizeRatioLimit_(sizeRatioLimit) {
        rooms_.push_back(Room{});  // index 0 unused
    }

    int roomSize() const { return static_cast<int>(rooms_.size()) - 1; }
    int lastMainId() const { return lastMainId_; }
    int branchCount() const { return static_cast<int>(branches_.size()); }

    Room& room(int id) { return rooms_[static_cast<size_t>(id)]; }
    const Room& room(int id) const { return rooms_[static_cast<size_t>(id)]; }

    int idAt(Pos p) const {
        auto it = posToId_.find(p);
        return it == posToId_.end() ? 0 : it->second;
    }

    int addRoom(int parentId, Pos pos, int branch) {
        Room r;
        r.id = roomSize() + 1;
        r.pos = pos;
        r.branch = branch;
        rooms_.push_back(r);
        posToId_[pos] = r.id;
        minX_ = std::min(minX_, pos.x);
        maxX_ = std::max(maxX_, pos.x);
        minY_ = std::min(minY_, pos.y);
        maxY_ = std::max(maxY_, pos.y);
        if (parentId > 0) {
            rooms_[static_cast<size_t>(r.id)].neighbors.push_back(parentId);
            rooms_[static_cast<size_t>(parentId)].neighbors.push_back(r.id);
        }
        if (branch == 0) {
            lastMainId_ = std::max(lastMainId_, r.id);
        }
        return r.id;
    }

    // 只能回退主线房间。支线开始生成后 roomIdSeq 和 lastMain 就对不上了。
    int popLastMainRoom() {
        if (lastMainId_ != roomSize() || lastMainId_ == 0) {
            return 0;
        }
        const int last = lastMainId_;
        Room dying = rooms_.back();
        for (int nb : dying.neighbors) {
            auto& nbs = rooms_[static_cast<size_t>(nb)].neighbors;
            nbs.erase(std::remove(nbs.begin(), nbs.end(), last), nbs.end());
        }
        posToId_.erase(dying.pos);
        rooms_.pop_back();
        --lastMainId_;
        // 弹栈后外框要重算，不然长宽比还按已经删掉的格子卡
        recomputeBounds();
        return lastMainId_;
    }

    std::vector<Pos> nextAvailable(Pos cur, const std::function<bool(Pos)>& filter) const {
        std::vector<Pos> out;
        for (const Pos dir : kDirs) {
            const Pos next{cur.x + dir.x, cur.y + dir.y};
            if (isPosAvailable(next, filter)) {
                out.push_back(next);
            }
        }
        return out;
    }

    // 格子能不能用：默认有房间就否；交汇时由 filter 自己决定。
    // 另外卡一下长宽比，别生成一条细长蛇。
    bool isPosAvailable(Pos next, const std::function<bool(Pos)>& filter) const {
        if (!filter) {
            // 未定义 filter，碰到任何已有房间都拒绝
            if (idAt(next) != 0) {
                return false;
            }
        } else if (!filter(next)) {
            return false;
        }

        if (sizeRatioLimit_ <= 0) {
            return true;
        }
        int newWidth = maxX_ - minX_;
        if (next.x > maxX_) {
            newWidth = next.x - minX_;
        } else if (next.x < minX_) {
            newWidth = maxX_ - next.x;
        }
        ++newWidth;

        int newHeight = maxY_ - minY_;
        if (next.y > maxY_) {
            newHeight = next.y - minY_;
        } else if (next.y < minY_) {
            newHeight = maxY_ - next.y;
        }
        ++newHeight;
        if (newWidth <= 0 || newHeight <= 0) {
            return false;
        }

        return !(newWidth / newHeight > sizeRatioLimit_ || newHeight / newWidth > sizeRatioLimit_);
    }

    void addBranch(Branch b) {
        computeDepth(b);
        branches_.push_back(std::move(b));
    }

    bool alreadyLinked(int a, int b) const {
        const auto& nbs = rooms_[static_cast<size_t>(a)].neighbors;
        return std::find(nbs.begin(), nbs.end(), b) != nbs.end();
    }

    // 试连 a-b：如果起点到 Boss 的最短路被抄到主线的 shortcutRatio 以下，就撤回。
    // Reject the edge if it short-circuits the main path below shortcutRatio.
    bool linkWouldShortcut(int a, int b, int mainRooms, double shortcutRatio) {
        if (alreadyLinked(a, b)) {
            const int hops = shortestHops(1, mainRooms);
            const int limitNodes = static_cast<int>(std::ceil(shortcutRatio * mainRooms));
            return hops >= 0 && (hops + 1) <= limitNodes;
        }
        rooms_[static_cast<size_t>(a)].neighbors.push_back(b);
        rooms_[static_cast<size_t>(b)].neighbors.push_back(a);
        const int limitNodes = static_cast<int>(std::ceil(shortcutRatio * mainRooms));
        const int hops = shortestHops(1, mainRooms);
        rooms_[static_cast<size_t>(a)].neighbors.pop_back();
        rooms_[static_cast<size_t>(b)].neighbors.pop_back();
        return hops >= 0 && (hops + 1) <= limitNodes;
    }

    void confirmLink(int a, int b) {
        if (alreadyLinked(a, b)) {
            return;
        }
        rooms_[static_cast<size_t>(a)].neighbors.push_back(b);
        rooms_[static_cast<size_t>(b)].neighbors.push_back(a);
    }

    int shortestHops(int from, int to) const {
        if (from == to) {
            return 0;
        }
        if (from <= 0 || to <= 0 || from >= static_cast<int>(rooms_.size()) ||
            to >= static_cast<int>(rooms_.size())) {
            return -1;
        }
        std::vector<int> dist(rooms_.size(), -1);
        std::queue<int> q;
        dist[static_cast<size_t>(from)] = 0;
        q.push(from);
        while (!q.empty()) {
            const int cur = q.front();
            q.pop();
            for (int nb : rooms_[static_cast<size_t>(cur)].neighbors) {
                if (nb <= 0 || nb >= static_cast<int>(rooms_.size()) || dist[static_cast<size_t>(nb)] >= 0) {
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

    // 从 start 走到门为止。open 是门前可通行区，blocks 是撞上的门。
    void visitReachable(int start, const std::function<bool(int)>& isBlock, std::vector<int>& open,
                        std::vector<int>& blocks) const {
        std::vector<char> seen(rooms_.size(), 0);
        std::queue<int> q;
        q.push(start);
        seen[static_cast<size_t>(start)] = 1;
        while (!q.empty()) {
            const int cur = q.front();
            q.pop();
            if (cur != start && isBlock(cur)) {
                blocks.push_back(cur);
                continue;
            }
            if (cur != start) {
                open.push_back(cur);
            }
            if (isBlock(cur) && cur != start) {
                continue;
            }
            for (int nb : rooms_[static_cast<size_t>(cur)].neighbors) {
                if (!seen[static_cast<size_t>(nb)]) {
                    seen[static_cast<size_t>(nb)] = 1;
                    q.push(nb);
                }
            }
        }
    }

    Dungeon finish(uint32_t seed) const {
        Dungeon d;
        d.seed = seed;
        d.lastMainId = lastMainId_;
        d.rooms = rooms_;
        return d;
    }

    bool fillContents(const Config& cfg);

    std::vector<Branch> branches_;

private:
    void computeDepth(Branch& b) {
        if (b.connectedRoomId == 0) {
            // 没有主动和其他房间相连，末端当"最远房间"
            if (b.path.empty()) {
                b.depth = 0;
                return;
            }
            b.innerRoomId = b.path.back();
            int best = -1;
            for (int id = 1; id <= lastMainId_; ++id) {
                const int hops = shortestHops(b.innerRoomId, id);
                if (hops >= 0 && (best < 0 || hops < best)) {
                    best = hops;
                }
            }
            b.depth = best < 0 ? 0 : static_cast<size_t>(best);
        } else if (b.connectedRoomId <= lastMainId_) {
            // 和主线相连，深度按 1/2 长度算
            b.depth = static_cast<int>(b.path.size() + 1) / 2;
            if (b.depth > 0 && b.depth <= static_cast<int>(b.path.size())) {
                b.innerRoomId = b.path[static_cast<size_t>(b.depth - 1)];
            }
        } else {
            auto it = std::find(b.path.begin(), b.path.end(), b.connectedRoomId);
            if (it != b.path.end()) {
                // 和自己连成环，最远点取环中央
                int depth = static_cast<int>(it - b.path.begin()) + 1;
                depth += (static_cast<int>(b.path.size()) - depth + 1) / 2;
                b.depth = depth;
                if (depth > 0 && depth <= static_cast<int>(b.path.size())) {
                    b.innerRoomId = b.path[static_cast<size_t>(depth - 1)];
                }
            } else {
                // 接到别的支线时深度不好定义，先记 0，门就不会落到这条上
                b.innerRoomId = 0;
                b.depth = 0;
            }
        }
    }

    int placeDoorEvent(Branch& b) {
        int roomId = 0;
        if (b.connectedRoomId == 0) {
            if (b.path.size() > 3) {
                // 最远房间前空 1 格，尽量不和宝箱房挨着
                roomId = b.path[static_cast<size_t>(rng_.uniform(0, static_cast<int>(b.path.size()) - 2))];
            } else if (!b.path.empty()) {
                // 太短就只能搁起点
                roomId = b.path.front();
            }
        } else {
            // 和其他路径相连时，放在"最内侧"
            roomId = b.innerRoomId;
        }
        b.doorRoomId = roomId;
        return roomId;
    }

    // 深度平方做权重，深支线更容易分到门
    std::vector<int> pickByDepthPow2(const std::map<int, int>& depths, int n) {
        struct Item {
            int id;
            int w;
        };
        std::vector<Item> items;
        for (const auto& [id, depth] : depths) {
            items.push_back(Item{id, depth * depth});
        }
        std::vector<int> out;
        while (static_cast<int>(out.size()) < n && !items.empty()) {
            int total = 0;
            for (const auto& it : items) {
                total += it.w;
            }
            int roll = rng_.uniform(0, total + 1);
            bool picked = false;
            for (size_t i = 0; i < items.size(); ++i) {
                roll -= items[i].w;
                if (roll <= 0) {
                    out.push_back(items[i].id);
                    items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
                    picked = true;
                    break;
                }
            }
            if (!picked) {
                out.push_back(items.back().id);
                items.pop_back();
            }
        }
        return out;
    }

    void recomputeBounds() {
        if (roomSize() <= 0) {
            minX_ = maxX_ = minY_ = maxY_ = 0;
            return;
        }
        minX_ = maxX_ = rooms_[1].pos.x;
        minY_ = maxY_ = rooms_[1].pos.y;
        for (int i = 2; i <= roomSize(); ++i) {
            const Pos p = rooms_[static_cast<size_t>(i)].pos;
            minX_ = std::min(minX_, p.x);
            maxX_ = std::max(maxX_, p.x);
            minY_ = std::min(minY_, p.y);
            maxY_ = std::max(maxY_, p.y);
        }
    }

    Rng& rng_;
    int sizeRatioLimit_ = 0;
    int minX_ = 0, minY_ = 0, maxX_ = 0, maxY_ = 0;
    int lastMainId_ = 0;
    std::vector<Room> rooms_;
    std::map<Pos, int> posToId_;
};

bool Builder::fillContents(const Config& cfg) {
    // 可用房间：去掉起点，以及 Boss 前一格（给玩家一点反应时间）
    std::vector<int> available;
    auto visitAll = [&](const std::function<void(int)>& cb) {
        for (int id = 1; id <= lastMainId_; ++id) {
            cb(id);
        }
        for (const auto& b : branches_) {
            for (int id : b.path) {
                cb(id);
            }
        }
    };
    visitAll([&](int id) {
        if (id == 1) {
            return;
        }
        if (id == lastMainId_ - 1) {
            return;
        }
        available.push_back(id);
    });
    rng_.shuffle(available);

    struct KeyData {
        int reqRoomId = 0;
    };
    std::map<int, KeyData> dropKeys;

    // matchId==0 随便挑一间；钥匙默认不和事件叠，怪物可以叠在钥匙房
    auto popAvailable = [&](int matchId, bool canOverlapKeys = false) -> int {
        if (available.empty()) {
            return 0;
        }
        if (matchId == 0) {
            for (auto it = available.begin(); it != available.end(); ++it) {
                if (canOverlapKeys || dropKeys.find(*it) == dropKeys.end()) {
                    const int id = *it;
                    available.erase(it);
                    return id;
                }
            }
            return 0;
        }
        auto it = std::find(available.begin(), available.end(), matchId);
        if (it == available.end()) {
            return 0;
        }
        if (canOverlapKeys || dropKeys.find(matchId) == dropKeys.end()) {
            available.erase(it);
            return matchId;
        }
        return 0;
    };

    int bossRoomId = 0;
    if (cfg.hasBoss) {
        // 提前占住 Boss 房，后面事件别再抢
        bossRoomId = popAvailable(lastMainId_);
        if (bossRoomId == 0) {
            return false;
        }
    }

    std::map<int, EventKind> events;

    std::vector<int> doorRoomIds;
    const int doorNum = cfg.doors;
    // 门：主线按段切，支线按深度加权，每条支线最多一扇
    if (doorNum > 0) {
        std::map<int, int> branchDepthMap;
        for (size_t i = 0; i < branches_.size(); ++i) {
            if (branches_[i].depth == 0) {
                continue;
            }
            branchDepthMap[static_cast<int>(i) + 1] = branches_[i].depth;
        }

        int mainRoomStart = 0;
        if (!branches_.empty()) {
            // 第一条支线之前不放门
            mainRoomStart = branches_.front().mainRoomId + 1;
        }
        // 没支线时，前 5 个房间也不放
        mainRoomStart = std::max(mainRoomStart, 6);
        const int mainRoomEnd = lastMainId_ - 2;
        int mainDepth = mainRoomEnd - mainRoomStart + 1;
        if (mainDepth > 0) {
            int branchIdx = -1;
            while (mainDepth > kDoorPerMainRooms) {
                mainDepth -= kDoorPerMainRooms;
                branchDepthMap[branchIdx--] = kDoorPerMainRooms;
            }
            branchDepthMap[branchIdx] = mainDepth;
        }

        auto picked = pickByDepthPow2(branchDepthMap, doorNum);
        for (int branchIdx : picked) {
            int doorRoomId = 0;
            if (branchIdx > 0) {
                doorRoomId = placeDoorEvent(branches_[static_cast<size_t>(branchIdx - 1)]);
            } else {
                const int depth = branchDepthMap[branchIdx];
                const int startRoomId = mainRoomStart + (-branchIdx - 1) * kDoorPerMainRooms;
                const int endRoomId = startRoomId + depth - 1;
                doorRoomId = rng_.uniform(startRoomId, endRoomId + 1);
            }
            if (doorRoomId <= 0 || doorRoomId >= static_cast<int>(rooms_.size())) {
                continue;
            }
            if (popAvailable(doorRoomId) == 0) {
                continue;
            }
            events[doorRoomId] = EventKind::Door;
            doorRoomIds.push_back(doorRoomId);
        }
    }

    if (!doorRoomIds.empty()) {
        // 钥匙必须落在"不经过该门也能走到"的区域，否则玩家会卡死。
        // Keys are placed in the pre-door reachable set so the layout always has a solution.
        std::map<int, int> openedDoors;
        do {
            std::vector<int> openRooms;
            std::vector<int> doorRooms;
            auto isBlock = [&](int roomId) {
                auto it = events.find(roomId);
                if (it == events.end()) {
                    return false;
                }
                if (openedDoors.count(roomId)) {
                    return false;
                }
                return it->second == EventKind::Door;
            };
            visitReachable(1, isBlock, openRooms, doorRooms);
            rng_.shuffle(doorRooms);
            rng_.shuffle(openRooms);

            bool opened = false;
            for (int doorRoomId : doorRooms) {
                int keyRoomId = 0;
                for (int roomId : openRooms) {
                    if (dropKeys.count(roomId) || events.count(roomId)) {
                        continue;
                    }
                    if (roomId <= lastMainId_ && doorRoomId <= lastMainId_ && roomId <= doorRoomId) {
                        // 主线上的门，钥匙不掉在门前面的主线房间
                        continue;
                    }
                    if (doorRoomId > lastMainId_) {
                        if (room(doorRoomId).branch == room(roomId).branch) {
                            // 支线门的钥匙不放在同一条支线里
                            continue;
                        }
                    }
                    keyRoomId = roomId;
                    dropKeys[roomId] = KeyData{doorRoomId};
                    break;
                }
                if (keyRoomId != 0) {
                    openedDoors[doorRoomId] = keyRoomId;
                    opened = true;
                    break;
                }
            }
            if (!opened) {
                break;
            }
        } while (static_cast<int>(openedDoors.size()) < static_cast<int>(doorRoomIds.size()));

        for (int roomId : doorRoomIds) {
            if (!openedDoors.count(roomId)) {
                // 找不到合法钥匙位就把这扇门拿掉，总比出死图好
                events.erase(roomId);
            }
        }
    }

    std::vector<int> branchEnds;
    std::vector<int> branchDeadEnds;
    for (const auto& b : branches_) {
        if (b.innerRoomId <= 0) {
            continue;
        }
        if (b.connectedRoomId == 0) {
            branchDeadEnds.push_back(b.innerRoomId);
        } else {
            branchEnds.push_back(b.innerRoomId);
        }
    }
    rng_.shuffle(branchDeadEnds);
    rng_.shuffle(branchEnds);
    // 死路优先给大宝箱，所以塞到 vector 尾部（后面 pop_back）
    branchEnds.insert(branchEnds.end(), branchDeadEnds.begin(), branchDeadEnds.end());

    std::vector<int> chestRooms;
    int superPlaced = 0;
    for (int i = 0; i < cfg.superChests && !branchEnds.empty(); ++i) {
        const int roomId = branchEnds.back();
        branchEnds.pop_back();
        if (popAvailable(roomId) == 0) {
            continue;
        }
        events[roomId] = EventKind::SuperChest;
        chestRooms.push_back(roomId);
        ++superPlaced;
    }

    int middleNum = cfg.middleChests + (cfg.superChests - superPlaced);  // 大宝箱放不下就降成中的
    while (middleNum > 0) {
        int roomId = 0;
        if (!branchEnds.empty()) {
            roomId = branchEnds.back();
            branchEnds.pop_back();
            roomId = popAvailable(roomId);
        }
        if (roomId == 0) {
            for (int id : available) {
                bool ok = true;
                for (int chest : chestRooms) {
                    const int dist = shortestHops(id, chest);
                    if (dist >= 0 && dist < 3) {
                        // 两个宝箱至少隔 3 步，别挤在一块
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    roomId = popAvailable(id);
                    break;
                }
            }
        }
        if (roomId == 0) {
            break;
        }
        events[roomId] = EventKind::MiddleChest;
        chestRooms.push_back(roomId);
        --middleNum;
    }

    for (int i = 0; i < cfg.smallChests; ++i) {
        const int roomId = popAvailable(0);
        if (roomId == 0) {
            break;
        }
        events[roomId] = EventKind::SmallChest;
        chestRooms.push_back(roomId);
    }

    struct EnemyInfo {
        EnemyKind kind = EnemyKind::None;
    };
    std::map<int, EnemyInfo> monsters;
    if (bossRoomId > 0) {
        monsters[bossRoomId] = EnemyInfo{EnemyKind::Boss};
    }

    int monsterNum = sampleRange(rng_, cfg.monsterNums);
    for (int i = 0; i < static_cast<int>(chestRooms.size()); ++i) {
        const int chestId = chestRooms[static_cast<size_t>(i)];
        const bool superChest = (i < superPlaced);
        const double monsterRate = superChest ? 1.0 : 0.8;
        double eliteRate = cfg.eliteRate;
        if (superChest) {
            eliteRate = 1.0;  // 超级宝箱邻格必出精英守卫
        } else {
            eliteRate *= 2.0;
        }
        if (!rng_.chance(monsterRate)) {
            continue;
        }
        EnemyKind kind = rng_.chance(eliteRate) ? EnemyKind::Elite : EnemyKind::Normal;
        auto nbs = room(chestId).neighbors;
        rng_.shuffle(nbs);
        // 距离相同就保持刚才的随机顺序
        std::stable_sort(nbs.begin(), nbs.end(), [&](int a, int b) {
            return shortestHops(1, a) < shortestHops(1, b);
        });
        for (int nb : nbs) {
            if (popAvailable(nb, true) > 0) {
                monsters[nb] = EnemyInfo{kind};
                --monsterNum;
                break;
            }
        }
    }

    if (monsterNum > 0) {
        auto temp = available;
        for (const auto& [id, _] : monsters) {
            for (int nb : room(id).neighbors) {
                temp.erase(std::remove(temp.begin(), temp.end(), nb), temp.end());
            }
        }
        while (monsterNum > 0) {
            int roomId = 0;
            if (!temp.empty()) {
                roomId = temp.back();
                temp.pop_back();
            }
            roomId = popAvailable(roomId, true);
            if (roomId == 0) {
                break;
            }
            for (int nb : room(roomId).neighbors) {
                temp.erase(std::remove(temp.begin(), temp.end(), nb), temp.end());
            }
            EnemyKind kind = rng_.chance(cfg.eliteRate) ? EnemyKind::Elite : EnemyKind::Normal;
            monsters[roomId] = EnemyInfo{kind};
            --monsterNum;
        }
    }

    for (const auto& [id, kind] : events) {
        room(id).event = kind;
    }
    for (const auto& [id, info] : monsters) {
        room(id).enemy = info.kind;
    }
    for (const auto& [id, key] : dropKeys) {
        room(id).keyOpens = key.reqRoomId;
    }
    return true;
}

bool generateMain(Builder& b, Rng& rng, int mainRooms) {
    Pos curPos{0, 0};
    int curRoom = b.addRoom(0, curPos, 0);

    // 选择栈：当前步走投无路就弹栈换方向。比一次随机走死胡同靠谱。

    struct Selection {
        std::vector<Pos> available;
        int selectIdx = 0;
    };
    std::vector<Selection> stack;

    while (b.roomSize() < mainRooms) {
        auto nextPoses = b.nextAvailable(curPos, [&](Pos pos) {
            if (b.idAt(pos) != 0) {
                return false;
            }
            // 前瞻一步：新格子至少还要有 2 个出口，不然主线容易把自己封死
            auto future = b.nextAvailable(pos, nullptr);
            return future.size() >= 2;
        });
        if (nextPoses.size() > 1) {
            rng.shuffle(nextPoses);
        }
        stack.push_back(Selection{std::move(nextPoses), 0});

        bool picked = false;
        while (!stack.empty()) {
            auto& last = stack.back();
            if (last.selectIdx < static_cast<int>(last.available.size())) {
                curPos = last.available[static_cast<size_t>(last.selectIdx++)];
                picked = true;
                break;
            }
            stack.pop_back();
            curRoom = b.popLastMainRoom();
            if (curRoom <= 0) {
                return false;
            }
        }
        if (!picked) {
            return false;
        }
        const int lastRoom = curRoom;
        curRoom = b.addRoom(lastRoom, curPos, 0);
    }
    return true;
}

void generateBranches(Builder& b, Rng& rng, int mainRooms, int remainRooms, int subPaths,
                      double shortcutRatio) {
    // 起点和 Boss 不挂支线
    subPaths = std::min(mainRooms - 2, subPaths);
    if (subPaths <= 0 || remainRooms <= 0) {
        return;
    }

    // 把主线切成若干段，支线尽量均匀挂上去。
    // e.g. 主线 16 房、3 条支线 → 大约挂在前/中/后三段。
    const int roomsPerSub = (mainRooms - 2) / subPaths;
    std::vector<int> sectionSizes(static_cast<size_t>(subPaths), roomsPerSub);
    const int roomsMod = (mainRooms - 2) % subPaths;
    if (roomsMod > 0) {
        std::vector<int> idxes(static_cast<size_t>(subPaths));
        for (int i = 0; i < subPaths; ++i) {
            idxes[static_cast<size_t>(i)] = i;
        }
        rng.shuffle(idxes);
        idxes.resize(static_cast<size_t>(roomsMod));
        for (int idx : idxes) {
            sectionSizes[static_cast<size_t>(idx)]++;
        }
    }

    std::vector<int> subPathSizes;
    // 每条支线先保底 50%，剩下的再随机撒，避免一条特别长一条就 1 格
    const int minSize = remainRooms / subPaths / 2;
    int leftover = remainRooms;
    for (int i = 0; i < subPaths; ++i) {
        subPathSizes.push_back(minSize);
        leftover -= minSize;
    }
    while (leftover > 0) {
        subPathSizes[static_cast<size_t>(rng.uniform(0, static_cast<int>(subPathSizes.size())))]++;
        --leftover;
    }
    // 先生成长的：短支线空间不够时，配额还能补给后面
    std::sort(subPathSizes.begin(), subPathSizes.end());

    int curRoomId = 2;
    for (int sectionSize : sectionSizes) {
        if (subPathSizes.empty()) {
            break;
        }
        std::vector<int> candidates;
        for (int id = curRoomId; id < curRoomId + sectionSize; ++id) {
            if (b.nextAvailable(b.room(id).pos, nullptr).empty()) {
                continue;
            }
            candidates.push_back(id);
        }
        if (candidates.empty()) {
            curRoomId += sectionSize;
            continue;
        }

        const int root = candidates[static_cast<size_t>(rng.uniform(0, static_cast<int>(candidates.size())))];
        int subRooms = subPathSizes.back();
        subPathSizes.pop_back();

        int lastRoom = root;
        Builder::Branch branch;
        branch.mainRoomId = root;
        int prevId = 0;
        int roomNum = 1;
        for (; roomNum <= subRooms; ++roomNum) {
            auto nextPoses = b.nextAvailable(b.room(lastRoom).pos, [&](Pos pos) {
                const int existing = b.idAt(pos);
                if (existing == 0) {
                    return true;
                }
                if (roomNum == 1) {
                    return false;  // 第一个支线房必须有独立格子
                }
                return existing != prevId;  // 后面允许接已有房间，但不能回头
            });
            nextPoses.erase(std::remove_if(nextPoses.begin(), nextPoses.end(),
                                           [&](Pos p) {
                                               const int id = b.idAt(p);
                                               return id == 1 || id == mainRooms;  // 不和起点 / Boss 直接相连
                                           }),
                            nextPoses.end());
            if (nextPoses.empty()) {
                break;
            }

            if (roomNum <= subRooms / 2) {
                // 前半段优先占空地，后半段再考虑并入已有路径
                std::vector<Pos> occupied;
                std::vector<Pos> empty;
                for (Pos p : nextPoses) {
                    if (b.idAt(p) != 0) {
                        occupied.push_back(p);
                    } else {
                        empty.push_back(p);
                    }
                }
                if (empty.size() > 1) {
                    rng.shuffle(empty);
                }
                if (occupied.size() > 2) {
                    rng.shuffle(occupied);
                }
                nextPoses = std::move(empty);
                nextPoses.insert(nextPoses.end(), occupied.begin(), occupied.end());
            } else {
                rng.shuffle(nextPoses);
            }

            bool added = false;
            bool connected = false;
            for (Pos nextPos : nextPoses) {
                const int existing = b.idAt(nextPos);
                if (existing != 0) {
                    if (b.alreadyLinked(lastRoom, existing) ||
                        b.linkWouldShortcut(lastRoom, existing, mainRooms, shortcutRatio)) {
                        // 已经连过，或一连就把主线抄近路，换下一个点
                        continue;
                    }
                    b.confirmLink(lastRoom, existing);
                    connected = true;
                    branch.connectedRoomId = existing;
                    prevId = lastRoom;
                    lastRoom = existing;
                    added = true;
                    break;
                }
                const int nid = b.addRoom(lastRoom, nextPos, b.branchCount() + 1);
                prevId = lastRoom;
                lastRoom = nid;
                added = true;
                branch.path.push_back(nid);
                break;
            }
            if (!added) {
                break;
            }
            if (connected) {
                break;
            }
        }
        b.addBranch(branch);
        // 这条支线没长够的房间数，随机补给还没生成的支线
        while (roomNum < subRooms && !subPathSizes.empty()) {
            subPathSizes[static_cast<size_t>(rng.uniform(0, static_cast<int>(subPathSizes.size())))]++;
            ++roomNum;
        }
        curRoomId += sectionSize;
    }
}

}  // namespace

std::string errorMessage(GenError err) {
    switch (err) {
        case GenError::MainPathStuck: return "main path generation stuck";
        case GenError::FillFailed: return "failed to place required contents";
        default: return "ok";
    }
}

GenResult generate(const Config& cfg, uint32_t seed) {
    GenResult result;
    for (int attempt = 0; attempt < std::max(1, cfg.maxRetries); ++attempt) {
        result.retries = attempt;
        // 同 seed 重试没有意义（生成是确定的），后面几次稍微偏一下
        const uint32_t attemptSeed =
            seed + static_cast<uint32_t>(attempt) * 0x9E3779B9u;
        Rng rng(attemptSeed);
        Builder builder(rng, cfg.sizeRatioLimit);
        const int mainRooms = sampleRange(rng, cfg.mainRooms);
        if (!generateMain(builder, rng, mainRooms)) {
            result.error = GenError::MainPathStuck;
            continue;
        }
        const int totalRooms = std::max(mainRooms, sampleRange(rng, cfg.totalRooms));
        const int subPaths = sampleRange(rng, cfg.subPaths);
        generateBranches(builder, rng, mainRooms, totalRooms - mainRooms, subPaths, cfg.shortcutRatio);
        if (!builder.fillContents(cfg)) {
            result.error = GenError::FillFailed;
            continue;
        }
        result.dungeon = builder.finish(seed);
        result.error = GenError::Ok;
        return result;
    }
    return result;
}

}  // namespace dungeon
