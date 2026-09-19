#include "Profile.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#ifdef PSP
#include <pspiofilemgr.h>
#endif

#ifdef PSP
constexpr const char* kPspProfiles = "ms0:/PSP/GAME/CSPSP/Profiles";
#endif

int xpThreshold(int level) {
    if (level < 1) {
        return 0;
    }
    return 50 * level * (level + 1);
}

const char* rarityLabel(int r) {
    static const char* k[] = {"?", "COMMON", "UNCOMMON", "RARE", "EPIC", "LEGEND"};
    if (r < 1 || r > 5) {
        return k[0];
    }
    return k[r];
}

int CareerProfile::level() const {
    int lv = 1;
    while (lv < 40 && xp >= xpThreshold(lv)) {
        ++lv;
    }
    return lv;
}

int CareerProfile::xpIntoLevel() const {
    const int lv = level();
    return xp - xpThreshold(lv - 1);
}

int CareerProfile::xpToNext() const {
    const int lv = level();
    return xpThreshold(lv) - xpThreshold(lv - 1);
}

float CareerProfile::resistMul() const {
    return clamp(1.0f - 0.028f * static_cast<float>(level() - 1), 0.62f, 1.0f);
}

float CareerProfile::speedMul() const {
    return clamp(1.0f + 0.032f * static_cast<float>(level() - 1), 1.0f, 1.42f);
}

bool CareerProfile::hasGun(WeaponId id) const {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= 64) {
        return false;
    }
    return guns[i] != 0;
}

void CareerProfile::unlockStarter() {
    guns[static_cast<int>(WeaponId::Knife)] = 1;
    guns[static_cast<int>(WeaponId::Glock)] = 1;
    guns[static_cast<int>(WeaponId::USP)] = 1;
    skins[0] = 1;
    if (loadout[0] == 0) {
        loadout[0] = static_cast<int>(WeaponId::Glock);
        loadout[1] = static_cast<int>(WeaponId::USP);
        loadout[2] = static_cast<int>(WeaponId::MP5);
    }
}

namespace {
bool mkdirp(const std::string& p) {
    struct stat st {};
    if (stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
#ifdef PSP
    return sceIoMkdir(p.c_str(), 0777) >= 0;
#else
    return mkdir(p.c_str(), 0755) == 0;
#endif
}

std::string safeName(const std::string& name) {
    std::string o;
    for (char c : name) {
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            o.push_back(c);
        }
        if (o.size() >= 8) {
            break;
        }
    }
    if (o.empty()) {
        o = "PLAYER";
    }
    return o;
}
} // namespace

bool ProfileStore::init(const std::string& dir) {
#ifdef PSP
    dir_ = kPspProfiles;
    sceIoMkdir("ms0:/PSP/GAME/CSPSP", 0777);
    sceIoMkdir(kPspProfiles, 0777);
#else
    dir_ = dir;
#endif
    mkdirp(dir_);
    loadSettings();
    refresh();
    return true;
}

namespace {
std::string trimName(const char* raw) {
    if (!raw) {
        return {};
    }
    std::string n = raw;
    while (!n.empty() && (n.back() == ' ' || n.back() == '\r' || n.back() == '\n' || n.back() == '\0')) {
        n.pop_back();
    }
    return n;
}

bool isProfileSav(const char* raw) {
    const std::string n = trimName(raw);
    if (n.size() < 5 || n[0] == '.') {
        return false;
    }
    const std::string low = n;
    if (low.size() >= 8 && (low.compare(0, 8, "settings") == 0 || low.compare(0, 8, "SETTINGS") == 0)) {
        return false;
    }
    const char* ext = n.c_str() + n.size() - 4;
    return ext[0] == '.' && (ext[1] == 'S' || ext[1] == 's') && (ext[2] == 'A' || ext[2] == 'a') &&
           (ext[3] == 'V' || ext[3] == 'v');
}

std::string canonSav(const char* raw) {
    std::string n = trimName(raw);
    if (n.size() >= 4) {
        n = n.substr(0, n.size() - 4);
    }
    for (char& c : n) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    if (n.empty()) {
        return {};
    }
    return n + ".SAV";
}

void addUnique(std::vector<std::string>& out, const std::string& n) {
    for (const auto& e : out) {
        if (e == n) {
            return;
        }
    }
    out.push_back(n);
}
} // namespace

void ProfileStore::scanDir(const std::string& dir, std::vector<std::string>& out) const {
#ifdef PSP
    SceIoDirent ent __attribute__((aligned(64)));
    SceUID uid = sceIoDopen(dir.c_str());
    if (uid >= 0) {
        std::memset(&ent, 0, sizeof(ent));
        while (sceIoDread(uid, &ent) > 0) {
            if (isProfileSav(ent.d_name)) {
                addUnique(out, canonSav(ent.d_name));
            }
            std::memset(&ent, 0, sizeof(ent));
        }
        sceIoDclose(uid);
    }
#endif
    DIR* d = opendir(dir.c_str());
    if (!d) {
        return;
    }
    while (dirent* e = readdir(d)) {
        if (isProfileSav(e->d_name)) {
            addUnique(out, canonSav(e->d_name));
        }
    }
    closedir(d);
}

void ProfileStore::refresh() {
    std::vector<std::string> found;
    scanDir(dir_, found);
#ifdef PSP
    if (found.empty()) {
        scanDir("Profiles", found);
        scanDir("./Profiles", found);
    }
#endif
    files_ = std::move(found);
    std::sort(files_.begin(), files_.end());
}

bool ProfileStore::create(const std::string& name) {
    CareerProfile p;
    p.unlockStarter();
    const std::string safe = safeName(name);
    std::snprintf(p.name, sizeof(p.name), "%s", safe.c_str());
    mkdirp(dir_);
    const std::string file = safe + ".SAV";
    if (!writeFile(joinPath(dir_, file), p)) {
        return false;
    }
    current_ = p;
    hasCurrent_ = true;
    refresh();
    currentIndex_ = -1;
    for (int i = 0; i < count(); ++i) {
        if (files_[static_cast<size_t>(i)] == file) {
            currentIndex_ = i;
            break;
        }
    }
    if (currentIndex_ < 0) {
        files_.push_back(file);
        currentIndex_ = count() - 1;
    }
    return true;
}

bool ProfileStore::loadIndex(int i) {
    if (i < 0 || i >= count()) {
        hasCurrent_ = false;
        currentIndex_ = -1;
        return false;
    }
    if (!readFile(joinPath(dir_, files_[static_cast<size_t>(i)]), current_)) {
        return false;
    }
    current_.unlockStarter();
    hasCurrent_ = true;
    currentIndex_ = i;
    return true;
}

bool ProfileStore::saveCurrent() const {
    if (!hasCurrent_ || currentIndex_ < 0 || currentIndex_ >= count()) {
        return false;
    }
    return writeFile(joinPath(dir_, files_[static_cast<size_t>(currentIndex_)]), current_);
}

bool ProfileStore::saveNamed(const CareerProfile& p) const {
    return writeFile(joinPath(dir_, std::string(p.name) + ".SAV"), p);
}

bool ProfileStore::unionHasGun(WeaponId id) const {
    CareerProfile tmp;
    for (const auto& f : files_) {
        if (readFile(joinPath(dir_, f), tmp) && tmp.hasGun(id)) {
            return true;
        }
    }
    return id == WeaponId::Knife || id == WeaponId::Glock;
}

bool ProfileStore::unionHasSkin(int skin) const {
    if (skin == 0) {
        return true;
    }
    CareerProfile tmp;
    for (const auto& f : files_) {
        if (readFile(joinPath(dir_, f), tmp) && skin >= 0 && skin < 8 && tmp.skins[skin]) {
            return true;
        }
    }
    return false;
}

int ProfileStore::maxLevel() const {
    int best = 1;
    CareerProfile tmp;
    for (const auto& f : files_) {
        if (readFile(joinPath(dir_, f), tmp)) {
            best = std::max(best, tmp.level());
        }
    }
    return best;
}

bool ProfileStore::readFile(const std::string& path, CareerProfile& out) const {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    CareerProfile p{};
    const size_t n = std::fread(&p, 1, sizeof(p), f);
    std::fclose(f);
    if (n < 32) {
        return false;
    }
    p.name[15] = 0;
    p.unlockStarter();
    out = p;
    return true;
}

bool ProfileStore::writeFile(const std::string& path, const CareerProfile& p) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) {
        const size_t n = std::fwrite(&p, 1, sizeof(p), f);
        std::fflush(f);
        std::fclose(f);
#ifdef PSP
        sceIoSync("ms0:", 0);
#endif
        if (n == sizeof(p)) {
            return true;
        }
    }
#ifdef PSP
    SceUID fd = sceIoOpen(path.c_str(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        return false;
    }
    const int n = sceIoWrite(fd, &p, static_cast<int>(sizeof(p)));
    sceIoClose(fd);
    sceIoSync("ms0:", 0);
    return n == static_cast<int>(sizeof(p));
#else
    return false;
#endif
}

std::string ProfileStore::settingsPath() const {
    std::string parent = dir_;
    const auto slash = parent.find_last_of("/\\");
    if (slash != std::string::npos) {
        parent = parent.substr(0, slash);
    }
    if (parent.empty()) {
        parent = ".";
    }
    return joinPath(parent, "settings.ini");
}

void ProfileStore::loadSettings() {
    FILE* f = std::fopen(settingsPath().c_str(), "rb");
    if (!f) {
        return;
    }
    char buf[256]{};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    (void)n;
    const char* p = std::strstr(buf, "scheme");
    if (!p) {
        return;
    }
    p = std::strchr(p, '=');
    if (!p) {
        return;
    }
    ++p;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (std::strncmp(p, "dpad", 4) == 0 || *p == '1') {
        controlScheme = 1;
    } else {
        controlScheme = 0;
    }
}

void ProfileStore::saveSettings() const {
    FILE* f = std::fopen(settingsPath().c_str(), "wb");
    if (!f) {
        return;
    }
    std::fprintf(f, "[controls]\nscheme=%s\n", controlScheme ? "dpad" : "analog");
    std::fflush(f);
    std::fclose(f);
#ifdef PSP
    sceIoSync("ms0:", 0);
#endif
}
