#include "dungeon/config.hpp"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace dungeon {
namespace {

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;
};

class Parser {
public:
    explicit Parser(std::string text) : t_(std::move(text)) {}

    Json parse() {
        skip();
        Json v = value();
        skip();
        return v;
    }

private:
    std::string t_;
    size_t i_ = 0;

    void skip() {
        while (i_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[i_]))) {
            ++i_;
        }
    }

    char peek() const { return i_ < t_.size() ? t_[i_] : '\0'; }

    char get() { return i_ < t_.size() ? t_[i_++] : '\0'; }

    Json value() {
        skip();
        const char c = peek();
        if (c == '{') {
            return object();
        }
        if (c == '[') {
            return array();
        }
        if (c == '"') {
            Json v;
            v.type = Json::String;
            v.s = string();
            return v;
        }
        if (c == 't' || c == 'f') {
            Json v;
            v.type = Json::Bool;
            v.b = boolean();
            return v;
        }
        Json v;
        v.type = Json::Number;
        v.n = number();
        return v;
    }

    Json object() {
        Json v;
        v.type = Json::Object;
        get();  // {
        skip();
        if (peek() == '}') {
            get();
            return v;
        }
        while (true) {
            skip();
            const std::string key = string();
            skip();
            if (get() != ':') {
                throw std::runtime_error("json: expected ':'");
            }
            v.obj[key] = value();
            skip();
            const char c = get();
            if (c == '}') {
                break;
            }
            if (c != ',') {
                throw std::runtime_error("json: expected ',' or '}'");
            }
        }
        return v;
    }

    Json array() {
        Json v;
        v.type = Json::Array;
        get();  // [
        skip();
        if (peek() == ']') {
            get();
            return v;
        }
        while (true) {
            v.arr.push_back(value());
            skip();
            const char c = get();
            if (c == ']') {
                break;
            }
            if (c != ',') {
                throw std::runtime_error("json: expected ',' or ']'");
            }
        }
        return v;
    }

    std::string string() {
        if (get() != '"') {
            throw std::runtime_error("json: expected string");
        }
        std::string out;
        while (i_ < t_.size()) {
            const char c = get();
            if (c == '"') {
                return out;
            }
            if (c == '\\' && i_ < t_.size()) {
                out.push_back(get());
            } else {
                out.push_back(c);
            }
        }
        throw std::runtime_error("json: unterminated string");
    }

    bool boolean() {
        if (t_.compare(i_, 4, "true") == 0) {
            i_ += 4;
            return true;
        }
        if (t_.compare(i_, 5, "false") == 0) {
            i_ += 5;
            return false;
        }
        throw std::runtime_error("json: expected bool");
    }

    double number() {
        const size_t start = i_;
        if (peek() == '-') {
            get();
        }
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            get();
        }
        if (peek() == '.') {
            get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                get();
            }
        }
        return std::stod(t_.substr(start, i_ - start));
    }
};

Range rangeFrom(const Json& v, Range fallback) {
    if (v.type == Json::Array && v.arr.size() >= 2) {
        return Range{static_cast<int>(v.arr[0].n), static_cast<int>(v.arr[1].n)};
    }
    if (v.type == Json::Number) {
        const int n = static_cast<int>(v.n);
        return Range{n, n};
    }
    return fallback;
}

int intFrom(const Json& v, int fallback) {
    return v.type == Json::Number ? static_cast<int>(v.n) : fallback;
}

double doubleFrom(const Json& v, double fallback) {
    return v.type == Json::Number ? v.n : fallback;
}

bool boolFrom(const Json& v, bool fallback) {
    if (v.type == Json::Bool) {
        return v.b;
    }
    if (v.type == Json::Number) {
        return v.n != 0;
    }
    return fallback;
}

}  // namespace

Config defaultConfig() { return Config{}; }

Config loadConfig(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open config: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const Json root = Parser(ss.str()).parse();
    if (root.type != Json::Object) {
        throw std::runtime_error("config root must be an object");
    }

    Config cfg = defaultConfig();
    auto get = [&](const char* key) -> const Json* {
        auto it = root.obj.find(key);
        return it == root.obj.end() ? nullptr : &it->second;
    };

    if (auto* v = get("mainRooms")) {
        cfg.mainRooms = rangeFrom(*v, cfg.mainRooms);
    }
    if (auto* v = get("totalRooms")) {
        cfg.totalRooms = rangeFrom(*v, cfg.totalRooms);
    }
    if (auto* v = get("subPaths")) {
        cfg.subPaths = rangeFrom(*v, cfg.subPaths);
    }
    if (auto* v = get("monsterNums")) {
        cfg.monsterNums = rangeFrom(*v, cfg.monsterNums);
    }
    if (auto* v = get("doors")) {
        cfg.doors = intFrom(*v, cfg.doors);
    }
    if (auto* v = get("superChests")) {
        cfg.superChests = intFrom(*v, cfg.superChests);
    }
    if (auto* v = get("middleChests")) {
        cfg.middleChests = intFrom(*v, cfg.middleChests);
    }
    if (auto* v = get("smallChests")) {
        cfg.smallChests = intFrom(*v, cfg.smallChests);
    }
    if (auto* v = get("hasBoss")) {
        cfg.hasBoss = boolFrom(*v, cfg.hasBoss);
    }
    if (auto* v = get("shortcutRatio")) {
        cfg.shortcutRatio = doubleFrom(*v, cfg.shortcutRatio);
    }
    if (auto* v = get("sizeRatioLimit")) {
        cfg.sizeRatioLimit = intFrom(*v, cfg.sizeRatioLimit);
    }
    if (auto* v = get("eliteRate")) {
        cfg.eliteRate = doubleFrom(*v, cfg.eliteRate);
    }

    if (auto* events = get("events")) {
        if (events->type == Json::Object) {
            auto ev = [&](const char* key, int& dst) {
                auto it = events->obj.find(key);
                if (it != events->obj.end()) {
                    dst = intFrom(it->second, dst);
                }
            };
            ev("DOOR", cfg.doors);
            ev("SUPER_CHEST", cfg.superChests);
            ev("MIDDLE_CHEST", cfg.middleChests);
            ev("SMALL_CHEST", cfg.smallChests);
        }
    }
    return cfg;
}

std::string saveConfig(const Config& cfg) {
    std::ostringstream o;
    o << "{\n"
      << "  \"mainRooms\": [" << cfg.mainRooms.min << ", " << cfg.mainRooms.max << "],\n"
      << "  \"totalRooms\": [" << cfg.totalRooms.min << ", " << cfg.totalRooms.max << "],\n"
      << "  \"subPaths\": [" << cfg.subPaths.min << ", " << cfg.subPaths.max << "],\n"
      << "  \"monsterNums\": [" << cfg.monsterNums.min << ", " << cfg.monsterNums.max << "],\n"
      << "  \"doors\": " << cfg.doors << ",\n"
      << "  \"superChests\": " << cfg.superChests << ",\n"
      << "  \"middleChests\": " << cfg.middleChests << ",\n"
      << "  \"smallChests\": " << cfg.smallChests << ",\n"
      << "  \"hasBoss\": " << (cfg.hasBoss ? "true" : "false") << ",\n"
      << "  \"shortcutRatio\": " << cfg.shortcutRatio << ",\n"
      << "  \"sizeRatioLimit\": " << cfg.sizeRatioLimit << ",\n"
      << "  \"eliteRate\": " << cfg.eliteRate << "\n"
      << "}\n";
    return o.str();
}

}  // namespace dungeon
