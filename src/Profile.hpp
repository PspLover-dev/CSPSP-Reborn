#pragma once

#include "Types.hpp"
#include "Weapons.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct CareerProfile {
    char name[16]{};
    int cash = 500;
    int xp = 0;
    int kills = 0;
    int deaths = 0;
    int matches = 0;
    int basesDestroyed = 0;
    int maxWave = 0;
    int nadeStock[kGrenadeTypes] = {2, 2, 1};
    uint8_t guns[64]{};
    uint8_t skins[8]{};
    int loadout[3] = {1, 2, 11};

    int level() const;
    int xpIntoLevel() const;
    int xpToNext() const;
    float resistMul() const;
    float speedMul() const;
    bool hasGun(WeaponId id) const;
    void unlockStarter();
};

class ProfileStore {
public:
    bool init(const std::string& dir);
    void refresh();
    bool create(const std::string& name);
    bool loadIndex(int i);
    bool saveCurrent() const;
    bool saveNamed(const CareerProfile& p) const;

    int count() const { return static_cast<int>(files_.size()); }
    const std::string& fileName(int i) const { return files_[static_cast<size_t>(i)]; }
    CareerProfile* current() { return hasCurrent_ ? &current_ : nullptr; }
    const CareerProfile* current() const { return hasCurrent_ ? &current_ : nullptr; }
    int currentIndex() const { return currentIndex_; }

    bool unionHasGun(WeaponId id) const;
    bool unionHasSkin(int skin) const;
    int maxLevel() const;
    uint8_t controlScheme = 0;
    void persistSettings() const { saveSettings(); }

private:
    bool readFile(const std::string& path, CareerProfile& out) const;
    bool writeFile(const std::string& path, const CareerProfile& p) const;
    void scanDir(const std::string& dir, std::vector<std::string>& out) const;
    std::string settingsPath() const;
    void loadSettings();
    void saveSettings() const;

    std::string dir_;
    std::vector<std::string> files_;
    CareerProfile current_{};
    bool hasCurrent_ = false;
    int currentIndex_ = -1;
};

int xpThreshold(int level);
const char* rarityLabel(int r);
