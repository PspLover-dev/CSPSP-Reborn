#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

struct MapEntry {
    std::string file;
    std::string size;
    std::string theme;
    std::string name;
    int unlockLevel = 0;
};

class GameMap {
public:
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    void createBlank(int w, int h, const std::string& sizeName, const std::string& theme, const std::string& name);

    int width() const { return w_; }
    int height() const { return h_; }
    float pixelW() const { return static_cast<float>(w_ * kTile); }
    float pixelH() const { return static_cast<float>(h_ * kTile); }

    Tile at(int x, int y) const;
    void set(int x, int y, Tile t);
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    bool solid(int x, int y) const;
    bool solidWorld(float px, float py, float radius) const;
    bool lineBlocked(Vec2 a, Vec2 b, float maxDist = 0.0f) const;

    Vec2 spawn(Team team, int slot, int teamCount = 2) const;
    std::vector<Vec2> spawns(Team team) const;
    Vec2 siteCenter(Team team) const;
    Vec2 teamHome(int teamIndex, int teamCount) const;
    void rebuildSpawns();

    const std::string& name() const { return name_; }
    const std::string& theme() const { return theme_; }
    const std::string& sizeName() const { return sizeName_; }
    int unlockLevel() const { return unlockLevel_; }
    void setUnlockLevel(int v) { unlockLevel_ = v; }
    void setName(const std::string& n) { name_ = n; }
    void setTheme(const std::string& t) { theme_ = t; }
    void setSizeName(const std::string& s) { sizeName_ = s; }

    static char tileToChar(Tile t);
    static Tile charToTile(char c);
    static std::vector<MapEntry> loadIndex(const std::string& mapsDir);
    static bool writeIndex(const std::string& mapsDir, const std::vector<MapEntry>& entries);
    static bool appendIndex(const std::string& mapsDir, const MapEntry& entry);
    void clear();

    std::string floorSprite() const;
    std::string wallSprite() const;
    std::string floorAltSprite() const;
    std::string csTile(const char* slot) const;
    const char* themeKey() const;

    static constexpr int kThemeCount = 10;
    static const char* themeName(int i);
    static int themeIndex(const std::string& name);

private:
    int w_ = 0;
    int h_ = 0;
    std::vector<Tile> tiles_;
    std::string name_ = "New Map";
    std::string theme_ = "dust";
    std::string sizeName_ = "small";
    int unlockLevel_ = 0;
    std::vector<Vec2> spawnT_;
    std::vector<Vec2> spawnCT_;
    std::vector<Vec2> siteA_;
    std::vector<Vec2> siteB_;
};
