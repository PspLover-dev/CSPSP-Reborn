#pragma once

#include <cmath>
#include <cstdint>
#include <string>

constexpr int kScreenW = 480;
constexpr int kScreenH = 272;
constexpr int kTile = 32;
constexpr int kMaxPlayers = 32;
constexpr float kPlayerRadius = 10.0f;
constexpr float kPlayerSpeed = 130.0f;
constexpr float kAnalogDeadzone = 0.18f;
constexpr float kPi = 3.14159265358979323846f;

constexpr int kMaxTeams = 6;
constexpr int kSkinCount = 8;

enum class Team : uint8_t {
    Terrorist = 0,
    Counter = 1,
    C = 2,
    D = 3,
    E = 4,
    F = 5
};

inline int teamIndex(Team t) {
    const int i = static_cast<int>(t);
    if (i < 0) {
        return 0;
    }
    if (i >= kMaxTeams) {
        return kMaxTeams - 1;
    }
    return i;
}

inline Team teamFromIndex(int i) {
    if (i < 0) {
        i = 0;
    }
    if (i >= kMaxTeams) {
        i = kMaxTeams - 1;
    }
    return static_cast<Team>(i);
}

inline const char* teamLetter(Team t) {
    static const char* kLetters[] = {"A", "B", "C", "D", "E", "F"};
    return kLetters[teamIndex(t)];
}

inline int maxTeamsForSize(const std::string& size) {
    if (size == "small") {
        return 2;
    }
    if (size == "medium") {
        return 3;
    }
    if (size == "big") {
        return 4;
    }
    return 6;
}

enum class Difficulty : uint8_t { Easy = 0, Medium = 1, Hard = 2 };

enum class GameMode : uint8_t {
    Normal = 0,
    Zombie = 1,
    SoloVsAll = 2,
    LastSurvivor = 3,
    ProtectBase = 4,
    Count = 5
};

inline const char* gameModeName(GameMode m) {
    switch (m) {
        case GameMode::Zombie:
            return "ZOMBIE SURVIVAL";
        case GameMode::SoloVsAll:
            return "SEUL CONTRE TOUS";
        case GameMode::LastSurvivor:
            return "LAST SURVIVOR";
        case GameMode::ProtectBase:
            return "PROTECT THE BASE";
        default:
            return "NORMAL";
    }
}

inline const char* gameModeHint(GameMode m) {
    switch (m) {
        case GameMode::Zombie:
            return "Allies vs zombie waves";
        case GameMode::SoloVsAll:
            return "You vs every bot";
        case GameMode::LastSurvivor:
            return "FFA, last one standing";
        case GameMode::ProtectBase:
            return "Destroy the enemy base first";
        default:
            return "Classic teams + bots";
    }
}

inline bool modeUsesTeams(GameMode m) {
    return m == GameMode::Normal || m == GameMode::ProtectBase;
}

inline bool modePicksTeamSkin(GameMode m) {
    return modeUsesTeams(m) || m == GameMode::Zombie;
}

enum class Tile : uint8_t {
    Floor = 0,
    Wall,
    Crate,
    Barrel,
    Door,
    Water,
    SpawnT,
    SpawnCT,
    SiteA,
    SiteB,
    Cover,
    Tree,
    Count
};

enum class GunKind : uint8_t { Primary = 0, Secondary = 1, Knife = 2, Grenade = 3 };

enum class WeaponId : uint8_t {
    Knife = 0,
    Glock,
    USP,
    P228,
    Deagle,
    FiveSeven,
    Elite,
    M3,
    XM1014,
    TMP,
    MAC10,
    MP5,
    UMP,
    P90,
    FAMAS,
    GALIL,
    Scout,
    M4A1,
    AK47,
    AUG,
    SG552,
    SG550,
    G3SG1,
    AWP,
    M249,
    Flashbang,
    HEGrenade,
    SmokeGrenade,
    Count
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float nx, float ny) : x(nx), y(ny) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o) {
        x += o.x;
        y += o.y;
        return *this;
    }

    float length() const { return std::sqrt(x * x + y * y); }
    float length2() const { return x * x + y * y; }

    Vec2 normalized() const {
        float l = length();
        if (l < 0.0001f) {
            return {0.0f, 0.0f};
        }
        return {x / l, y / l};
    }
};

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float deg(float rad) { return rad * 180.0f / kPi; }
inline float rad(float d) { return d * kPi / 180.0f; }
inline float angleOf(const Vec2& v) { return std::atan2(v.y, v.x); }
inline float lerpAngle(float from, float to, float t) {
    float d = to - from;
    while (d > kPi) {
        d -= 2.0f * kPi;
    }
    while (d < -kPi) {
        d += 2.0f * kPi;
    }
    return from + d * clamp(t, 0.0f, 1.0f);
}

inline std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (a.back() == '/' || a.back() == '\\') {
        return a + b;
    }
    return a + "/" + b;
}
