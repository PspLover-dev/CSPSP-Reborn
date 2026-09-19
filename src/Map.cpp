#include "Map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef PSP
#include <pspiofilemgr.h>
#endif

char GameMap::tileToChar(Tile t) {
    switch (t) {
        case Tile::Wall:
            return '#';
        case Tile::Crate:
            return 'C';
        case Tile::Barrel:
            return 'B';
        case Tile::Door:
            return 'D';
        case Tile::Water:
            return '~';
        case Tile::SpawnT:
            return 'T';
        case Tile::SpawnCT:
            return 'O';
        case Tile::SiteA:
            return 'A';
        case Tile::SiteB:
            return 'X';
        case Tile::Cover:
            return 'S';
        case Tile::Tree:
            return 'R';
        case Tile::Nuclear:
            return 'N';
        default:
            return '.';
    }
}

Tile GameMap::charToTile(char c) {
    switch (c) {
        case '#':
            return Tile::Wall;
        case 'C':
            return Tile::Crate;
        case 'B':
            return Tile::Barrel;
        case 'D':
            return Tile::Door;
        case '~':
            return Tile::Water;
        case 'T':
            return Tile::SpawnT;
        case 'O':
            return Tile::SpawnCT;
        case 'A':
            return Tile::SiteA;
        case 'X':
            return Tile::SiteB;
        case 'S':
            return Tile::Cover;
        case 'R':
            return Tile::Tree;
        case 'N':
            return Tile::Nuclear;
        default:
            return Tile::Floor;
    }
}

bool GameMap::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string magic;
    int ver = 0;
    in >> magic >> ver;
    if (magic != "CSPSP") {
        return false;
    }
    std::string key;
    w_ = h_ = 0;
    while (in >> key) {
        if (key == "name") {
            std::getline(in, name_);
            if (!name_.empty() && name_[0] == ' ') {
                name_.erase(0, 1);
            }
        } else if (key == "theme") {
            in >> theme_;
        } else if (key == "size") {
            in >> sizeName_;
        } else if (key == "unlock") {
            in >> unlockLevel_;
        } else if (key == "w") {
            in >> w_;
        } else if (key == "h") {
            in >> h_;
            break;
        }
    }
    std::string rest;
    std::getline(in, rest); // end of h line
    tiles_.assign(static_cast<size_t>(w_ * h_), Tile::Floor);
    for (int y = 0; y < h_; ++y) {
        std::string row;
        if (!std::getline(in, row)) {
            break;
        }
        if (!row.empty() && row.back() == '\r') {
            row.pop_back();
        }
        for (int x = 0; x < w_ && x < static_cast<int>(row.size()); ++x) {
            tiles_[y * w_ + x] = charToTile(row[static_cast<size_t>(x)]);
        }
    }
    rebuildSpawns();
    return w_ > 0 && h_ > 0;
}

bool GameMap::save(const std::string& path) const {
    if (w_ <= 0 || h_ <= 0 || tiles_.empty()) {
        return false;
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    if (std::fprintf(f, "CSPSP 1\nname %s\ntheme %s\nsize %s\nunlock %d\nw %d\nh %d\n", name_.c_str(), theme_.c_str(),
                     sizeName_.c_str(), unlockLevel_, w_, h_) < 0) {
        std::fclose(f);
        return false;
    }
    std::string row(static_cast<size_t>(w_) + 1, '\n');
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            row[static_cast<size_t>(x)] = tileToChar(at(x, y));
        }
        if (std::fwrite(row.data(), 1, row.size(), f) != row.size()) {
            std::fclose(f);
            return false;
        }
    }
    std::fflush(f);
    std::fclose(f);
#ifdef PSP
    sceIoSync("ms0:", 0);
#endif
    FILE* check = std::fopen(path.c_str(), "rb");
    if (!check) {
        return false;
    }
    char magic[8]{};
    const size_t n = std::fread(magic, 1, 5, check);
    std::fclose(check);
    return n == 5 && std::strncmp(magic, "CSPSP", 5) == 0;
}

void GameMap::createBlank(int w, int h, const std::string& sizeName, const std::string& theme, const std::string& name) {
    w_ = w;
    h_ = h;
    sizeName_ = sizeName;
    theme_ = theme;
    name_ = name;
    tiles_.assign(static_cast<size_t>(w * h), Tile::Floor);
    for (int x = 0; x < w; ++x) {
        set(x, 0, Tile::Wall);
        set(x, h - 1, Tile::Wall);
    }
    for (int y = 0; y < h; ++y) {
        set(0, y, Tile::Wall);
        set(w - 1, y, Tile::Wall);
    }
    int n = 4;
    if (w >= 100) {
        n = 8;
    }
    if (w >= 200) {
        n = 12;
    }
    if (w >= 400) {
        n = 16;
    }
    for (int i = 0; i < n; ++i) {
        const int t = 2 + i * 2;
        const int c = w - 3 - i * 2;
        if (t > 0 && t < w - 1) {
            set(t, 2, Tile::SpawnT);
            set(t, 3, Tile::SpawnT);
        }
        if (c > 0 && c < w - 1) {
            set(c, h - 3, Tile::SpawnCT);
            set(c, h - 4, Tile::SpawnCT);
        }
    }
    rebuildSpawns();
}

Tile GameMap::at(int x, int y) const {
    if (!inBounds(x, y)) {
        return Tile::Wall;
    }
    return tiles_[static_cast<size_t>(y * w_ + x)];
}

void GameMap::set(int x, int y, Tile t) {
    if (inBounds(x, y)) {
        tiles_[static_cast<size_t>(y * w_ + x)] = t;
    }
}

bool GameMap::solid(int x, int y) const {
    switch (at(x, y)) {
        case Tile::Wall:
        case Tile::Crate:
        case Tile::Barrel:
        case Tile::Tree:
        case Tile::Cover:
        case Tile::Nuclear:
            return true;
        default:
            return false;
    }
}

bool GameMap::solidWorld(float px, float py, float radius) const {
    const float r = radius;
    const float samples[5][2] = {{0.0f, 0.0f}, {-r, -r}, {r, -r}, {-r, r}, {r, r}};
    for (auto& s : samples) {
        const int tx = static_cast<int>((px + s[0]) / kTile);
        const int ty = static_cast<int>((py + s[1]) / kTile);
        if (solid(tx, ty)) {
            return true;
        }
    }
    return false;
}

bool GameMap::lineBlocked(Vec2 a, Vec2 b, float maxDist) const {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist2 = dx * dx + dy * dy;
    if (dist2 < 1.0f) {
        return false;
    }
    if (maxDist > 0.0f && dist2 > maxDist * maxDist) {
        return true;
    }
    int x0 = static_cast<int>(a.x / kTile);
    int y0 = static_cast<int>(a.y / kTile);
    int x1 = static_cast<int>(b.x / kTile);
    int y1 = static_cast<int>(b.y / kTile);
    int x = x0;
    int y = y0;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int dxAbs = std::abs(x1 - x0);
    int dyAbs = std::abs(y1 - y0);
    int err = dxAbs - dyAbs;
    const int limit = dxAbs + dyAbs + 1;
    for (int i = 0; i < limit; ++i) {
        if ((x != x0 || y != y0) && solid(x, y)) {
            return true;
        }
        if (x == x1 && y == y1) {
            break;
        }
        const int e2 = err * 2;
        if (e2 > -dyAbs) {
            err -= dyAbs;
            x += sx;
        }
        if (e2 < dxAbs) {
            err += dxAbs;
            y += sy;
        }
    }
    return false;
}

void GameMap::rebuildSpawns() {
    spawnT_.clear();
    spawnCT_.clear();
    siteA_.clear();
    siteB_.clear();
    spawnT_.reserve(32);
    spawnCT_.reserve(32);
    siteA_.reserve(16);
    siteB_.reserve(16);
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            const Tile t = at(x, y);
            const Vec2 c{x * kTile + kTile * 0.5f, y * kTile + kTile * 0.5f};
            if (t == Tile::SpawnT) {
                spawnT_.push_back(c);
            } else if (t == Tile::SpawnCT) {
                spawnCT_.push_back(c);
            } else if (t == Tile::SiteA) {
                siteA_.push_back(c);
            } else if (t == Tile::SiteB) {
                siteB_.push_back(c);
            }
        }
    }
}

Vec2 GameMap::siteCenter(Team team) const {
    const std::vector<Vec2>& sites = team == Team::Terrorist ? siteA_ : siteB_;
    if (!sites.empty()) {
        Vec2 s{};
        for (const auto& p : sites) {
            s += p;
        }
        const float n = static_cast<float>(sites.size());
        return {s.x / n, s.y / n};
    }
    const std::vector<Vec2>& sp = team == Team::Terrorist ? spawnT_ : spawnCT_;
    if (!sp.empty()) {
        Vec2 s{};
        for (const auto& p : sp) {
            s += p;
        }
        const float n = static_cast<float>(sp.size());
        return {s.x / n, s.y / n};
    }
    if (team == Team::Terrorist) {
        return {kTile * 3.5f, pixelH() * 0.5f};
    }
    return {pixelW() - kTile * 3.5f, pixelH() * 0.5f};
}

std::vector<Vec2> GameMap::spawns(Team team) const {
    const std::vector<Vec2>& src = team == Team::Terrorist ? spawnT_ : spawnCT_;
    if (!src.empty()) {
        return src;
    }
    return {{kTile * 2.0f, kTile * 2.0f}};
}

Vec2 GameMap::teamHome(int idx, int teamCount) const {
    const int n = std::max(2, teamCount);
    idx = ((idx % n) + n) % n;
    auto avg = [](const std::vector<Vec2>& list) {
        Vec2 s{};
        for (const auto& p : list) {
            s += p;
        }
        const float c = static_cast<float>(list.size());
        return Vec2{s.x / c, s.y / c};
    };
    auto nearestOpen = [this](Vec2 want) {
        want.x = clamp(want.x, kTile * 2.5f, pixelW() - kTile * 2.5f);
        want.y = clamp(want.y, kTile * 2.5f, pixelH() - kTile * 2.5f);
        auto tileCenterOf = [this](int tx, int ty) {
            return Vec2{tx * kTile + kTile * 0.5f, ty * kTile + kTile * 0.5f};
        };
        int sx = static_cast<int>(want.x / kTile);
        int sy = static_cast<int>(want.y / kTile);
        sx = std::max(1, std::min(w_ - 2, sx));
        sy = std::max(1, std::min(h_ - 2, sy));
        if (!solid(sx, sy)) {
            return tileCenterOf(sx, sy);
        }
        const int maxR = std::min(80, std::max(w_, h_) / 2);
        for (int r = 1; r <= maxR; ++r) {
            for (int dx = -r; dx <= r; ++dx) {
                const int tx = sx + dx;
                const int ty0 = sy - r;
                const int ty1 = sy + r;
                if (inBounds(tx, ty0) && !solid(tx, ty0)) {
                    return tileCenterOf(tx, ty0);
                }
                if (inBounds(tx, ty1) && !solid(tx, ty1)) {
                    return tileCenterOf(tx, ty1);
                }
            }
            for (int dy = -r + 1; dy <= r - 1; ++dy) {
                const int ty = sy + dy;
                const int tx0 = sx - r;
                const int tx1 = sx + r;
                if (inBounds(tx0, ty) && !solid(tx0, ty)) {
                    return tileCenterOf(tx0, ty);
                }
                if (inBounds(tx1, ty) && !solid(tx1, ty)) {
                    return tileCenterOf(tx1, ty);
                }
            }
        }
        return want;
    };
    if (idx == 0 && !siteA_.empty()) {
        return nearestOpen(avg(siteA_));
    }
    if (idx == 1 && !siteB_.empty()) {
        return nearestOpen(avg(siteB_));
    }
    if (idx == 0 && !spawnT_.empty()) {
        return nearestOpen(avg(spawnT_));
    }
    if (idx == 1 && !spawnCT_.empty()) {
        return nearestOpen(avg(spawnCT_));
    }
    const float ang = (kPi * 2.0f * static_cast<float>(idx)) / static_cast<float>(n) - kPi * 0.5f;
    const Vec2 mid{pixelW() * 0.5f, pixelH() * 0.5f};
    const float rx = std::max(kTile * 5.0f, pixelW() * 0.36f);
    const float ry = std::max(kTile * 5.0f, pixelH() * 0.36f);
    return nearestOpen({mid.x + std::cos(ang) * rx, mid.y + std::sin(ang) * ry});
}

Vec2 GameMap::spawn(Team team, int slot, int teamCount) const {
    const int idx = teamIndex(team);
    if (idx == 0 && !spawnT_.empty()) {
        return spawnT_[static_cast<size_t>(slot) % spawnT_.size()];
    }
    if (idx == 1 && !spawnCT_.empty()) {
        return spawnCT_[static_cast<size_t>(slot) % spawnCT_.size()];
    }
    const Vec2 h = teamHome(idx, teamCount);
    const float ring = 22.0f + static_cast<float>(slot % 6) * 8.0f;
    const float ang = static_cast<float>(slot) * 0.95f;
    const Vec2 p{h.x + std::cos(ang) * ring, h.y + std::sin(ang) * ring};
    if (!solidWorld(p.x, p.y, kPlayerRadius)) {
        return p;
    }
    return h;
}

std::vector<MapEntry> GameMap::loadIndex(const std::string& mapsDir) {
    std::vector<MapEntry> entries;
    std::ifstream in(joinPath(mapsDir, "index.txt"));
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        MapEntry e;
        std::istringstream ss(line);
        if (!(ss >> e.file >> e.size >> e.theme)) {
            continue;
        }
        std::getline(ss, e.name);
        if (!e.name.empty() && e.name[0] == '\t') {
            e.name.erase(0, 1);
        }
        if (!e.name.empty() && e.name[0] == ' ') {
            e.name.erase(0, 1);
        }
        // index uses tabs
        auto tab = line.find('\t');
        if (tab != std::string::npos) {
            std::istringstream ts(line);
            std::getline(ts, e.file, '\t');
            std::getline(ts, e.size, '\t');
            std::getline(ts, e.theme, '\t');
            std::getline(ts, e.name);
        }
        GameMap tmp;
        if (tmp.load(joinPath(mapsDir, e.file))) {
            e.unlockLevel = tmp.unlockLevel();
            if (e.name.empty()) {
                e.name = tmp.name();
            }
        }
        entries.push_back(e);
    }
    return entries;
}

bool GameMap::writeIndex(const std::string& mapsDir, const std::vector<MapEntry>& entries) {
    FILE* f = std::fopen(joinPath(mapsDir, "index.txt").c_str(), "wb");
    if (!f) {
        return false;
    }
    for (const auto& e : entries) {
        if (std::fprintf(f, "%s\t%s\t%s\t%s\n", e.file.c_str(), e.size.c_str(), e.theme.c_str(), e.name.c_str()) < 0) {
            std::fclose(f);
            return false;
        }
    }
    std::fflush(f);
    std::fclose(f);
#ifdef PSP
    sceIoSync("ms0:", 0);
#endif
    return true;
}

bool GameMap::appendIndex(const std::string& mapsDir, const MapEntry& entry) {
    auto existing = loadIndex(mapsDir);
    for (const auto& e : existing) {
        if (e.file == entry.file) {
            return true;
        }
    }
    FILE* f = std::fopen(joinPath(mapsDir, "index.txt").c_str(), "ab");
    if (!f) {
        return false;
    }
    const int ok = std::fprintf(f, "%s\t%s\t%s\t%s\n", entry.file.c_str(), entry.size.c_str(), entry.theme.c_str(),
                                entry.name.c_str());
    std::fflush(f);
    std::fclose(f);
#ifdef PSP
    sceIoSync("ms0:", 0);
#endif
    return ok >= 0;
}

void GameMap::clear() {
    tiles_.clear();
    tiles_.shrink_to_fit();
    spawnT_.clear();
    spawnCT_.clear();
    siteA_.clear();
    siteB_.clear();
    w_ = 0;
    h_ = 0;
}

const char* GameMap::themeName(int i) {
    static const char* kThemes[] = {"dust", "office", "inferno", "nuke", "vertigo",
                                    "cache", "aztec", "italy", "mill", "warehouse"};
    if (i < 0) {
        i = 0;
    }
    i %= kThemeCount;
    if (i < 0) {
        i += kThemeCount;
    }
    return kThemes[i];
}

int GameMap::themeIndex(const std::string& name) {
    for (int i = 0; i < kThemeCount; ++i) {
        if (name == themeName(i)) {
            return i;
        }
    }
    return 0;
}

const char* GameMap::themeKey() const {
    return themeName(themeIndex(theme_));
}

std::string GameMap::csTile(const char* slot) const {
    return std::string("cs_") + themeKey() + "_" + slot;
}

std::string GameMap::floorSprite() const { return csTile("floor"); }

std::string GameMap::wallSprite() const { return csTile("wall"); }

std::string GameMap::floorAltSprite() const { return csTile("alt"); }
