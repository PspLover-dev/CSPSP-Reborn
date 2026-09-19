#pragma once

#include "Types.hpp"

#include <cstdio>
#include <string>

struct WeaponDef {
    const char* name;
    const char* pose;
    GunKind kind;
    float damage;
    float rpm;
    float spread;
    float bulletSpeed;
    float range;
    float reload;
    float shake;
    float recoil;
    int mag;
    int reserve;
    int pellets;
    bool automatic;
    bool melee;
    int rarity;
};

inline const WeaponDef& weaponDef(WeaponId id) {
    static const WeaponDef kDefs[] = {
        {"KNIFE", "hold", GunKind::Knife, 40.0f, 200.0f, 0.0f, 0.0f, 22.0f, 0.2f, 1.5f, 0.0f, 1, 1, 1, false, true, 1},
        {"GLOCK", "gun", GunKind::Secondary, 22.0f, 600.0f, 0.07f, 520.0f, 280.0f, 2.1f, 3.5f, 0.04f, 20, 80, 1, false, false, 1},
        {"USP", "gun", GunKind::Secondary, 30.0f, 600.0f, 0.05f, 520.0f, 290.0f, 1.9f, 4.0f, 0.04f, 12, 72, 1, false, false, 1},
        {"P228", "gun", GunKind::Secondary, 36.0f, 600.0f, 0.07f, 520.0f, 290.0f, 2.0f, 4.5f, 0.05f, 13, 65, 1, false, false, 2},
        {"DEAGLE", "gun", GunKind::Secondary, 54.0f, 600.0f, 0.09f, 520.0f, 320.0f, 1.6f, 7.5f, 0.10f, 7, 42, 1, false, false, 3},
        {"FIVESEVEN", "gun", GunKind::Secondary, 30.0f, 600.0f, 0.03f, 520.0f, 290.0f, 2.2f, 3.5f, 0.03f, 20, 120, 1, false, false, 2},
        {"ELITE", "gun", GunKind::Secondary, 30.0f, 600.0f, 0.07f, 520.0f, 280.0f, 3.8f, 4.0f, 0.05f, 30, 150, 1, false, false, 2},
        {"M3", "gun", GunKind::Primary, 16.0f, 55.0f, 0.16f, 480.0f, 150.0f, 4.0f, 8.0f, 0.10f, 8, 40, 6, false, false, 2},
        {"XM1014", "gun", GunKind::Primary, 16.0f, 240.0f, 0.14f, 480.0f, 160.0f, 2.5f, 8.0f, 0.10f, 7, 35, 6, false, false, 3},
        {"TMP", "silencer", GunKind::Primary, 22.0f, 706.0f, 0.12f, 500.0f, 240.0f, 1.6f, 3.0f, 0.04f, 30, 150, 1, true, false, 1},
        {"MAC-10", "silencer", GunKind::Primary, 22.0f, 706.0f, 0.12f, 500.0f, 240.0f, 1.85f, 3.2f, 0.04f, 30, 120, 1, true, false, 1},
        {"MP5", "silencer", GunKind::Primary, 26.0f, 667.0f, 0.09f, 500.0f, 260.0f, 1.7f, 3.0f, 0.03f, 30, 150, 1, true, false, 2},
        {"UMP", "silencer", GunKind::Primary, 32.0f, 600.0f, 0.08f, 500.0f, 270.0f, 1.9f, 3.5f, 0.04f, 25, 125, 1, true, false, 2},
        {"P90", "silencer", GunKind::Primary, 24.0f, 857.0f, 0.09f, 500.0f, 260.0f, 1.9f, 3.2f, 0.03f, 50, 150, 1, true, false, 3},
        {"FAMAS", "machine", GunKind::Primary, 36.0f, 923.0f, 0.09f, 580.0f, 340.0f, 2.0f, 4.5f, 0.04f, 25, 100, 1, true, false, 3},
        {"GALIL", "machine", GunKind::Primary, 38.0f, 667.0f, 0.09f, 580.0f, 340.0f, 1.9f, 5.0f, 0.05f, 35, 140, 1, true, false, 3},
        {"SCOUT", "silencer", GunKind::Primary, 80.0f, 50.0f, 0.05f, 780.0f, 720.0f, 1.5f, 10.0f, 0.12f, 10, 100, 1, false, false, 3},
        {"M4A1", "machine", GunKind::Primary, 36.0f, 600.0f, 0.06f, 600.0f, 360.0f, 2.1f, 4.5f, 0.04f, 30, 120, 1, true, false, 3},
        {"AK-47", "machine", GunKind::Primary, 42.0f, 600.0f, 0.08f, 580.0f, 360.0f, 2.1f, 6.0f, 0.06f, 30, 120, 1, true, false, 3},
        {"AUG", "machine", GunKind::Primary, 36.0f, 632.0f, 0.07f, 600.0f, 380.0f, 2.5f, 4.5f, 0.04f, 30, 120, 1, true, false, 3},
        {"SG552", "machine", GunKind::Primary, 36.0f, 632.0f, 0.07f, 600.0f, 370.0f, 2.4f, 4.8f, 0.05f, 30, 120, 1, true, false, 3},
        {"SG550", "silencer", GunKind::Primary, 80.0f, 240.0f, 0.10f, 760.0f, 680.0f, 2.2f, 9.0f, 0.10f, 30, 120, 1, false, false, 4},
        {"G3SG1", "silencer", GunKind::Primary, 80.0f, 200.0f, 0.10f, 760.0f, 680.0f, 3.2f, 9.0f, 0.10f, 20, 100, 1, false, false, 4},
        {"AWP", "silencer", GunKind::Primary, 110.0f, 43.0f, 0.02f, 800.0f, 760.0f, 2.3f, 14.0f, 0.16f, 10, 40, 1, false, false, 5},
        {"M249", "machine", GunKind::Primary, 36.0f, 632.0f, 0.12f, 560.0f, 400.0f, 4.2f, 6.0f, 0.06f, 100, 200, 1, true, false, 4},
        {"FLASHBANG", "hold", GunKind::Grenade, 0.0f, 60.0f, 0.0f, 180.0f, 200.0f, 1.0f, 2.0f, 0.0f, 1, 0, 1, false, false, 2},
        {"HE-GRENADE", "hold", GunKind::Grenade, 100.0f, 60.0f, 0.0f, 200.0f, 210.0f, 1.0f, 8.0f, 0.0f, 1, 0, 1, false, false, 2},
        {"SMOKE", "hold", GunKind::Grenade, 0.0f, 60.0f, 0.0f, 160.0f, 190.0f, 1.0f, 2.0f, 0.0f, 1, 0, 1, false, false, 2},
        {"MP7", "silencer", GunKind::Primary, 28.0f, 750.0f, 0.08f, 520.0f, 270.0f, 1.8f, 3.2f, 0.03f, 30, 150, 1, true, false, 2},
        {"MP9", "silencer", GunKind::Primary, 24.0f, 860.0f, 0.10f, 510.0f, 250.0f, 1.6f, 3.0f, 0.03f, 30, 150, 1, true, false, 2},
        {"BIZON", "silencer", GunKind::Primary, 22.0f, 750.0f, 0.11f, 500.0f, 240.0f, 2.0f, 3.0f, 0.03f, 64, 192, 1, true, false, 2},
        {"VECTOR", "silencer", GunKind::Primary, 26.0f, 1100.0f, 0.07f, 530.0f, 250.0f, 1.7f, 3.4f, 0.03f, 25, 125, 1, true, false, 3},
        {"TEC-9", "gun", GunKind::Secondary, 32.0f, 500.0f, 0.10f, 520.0f, 270.0f, 2.0f, 4.2f, 0.06f, 18, 90, 1, false, false, 2},
        {"CZ75", "gun", GunKind::Secondary, 34.0f, 600.0f, 0.08f, 520.0f, 280.0f, 2.1f, 4.4f, 0.05f, 12, 60, 1, false, false, 2},
        {"R8", "gun", GunKind::Secondary, 70.0f, 150.0f, 0.06f, 560.0f, 340.0f, 1.8f, 8.5f, 0.12f, 8, 40, 1, false, false, 3},
        {"NOVA", "gun", GunKind::Primary, 18.0f, 70.0f, 0.15f, 470.0f, 150.0f, 3.6f, 7.5f, 0.10f, 8, 32, 6, false, false, 2},
        {"MAG-7", "gun", GunKind::Primary, 20.0f, 80.0f, 0.14f, 460.0f, 140.0f, 2.8f, 8.0f, 0.11f, 5, 32, 6, false, false, 3},
        {"SAWED", "gun", GunKind::Primary, 20.0f, 75.0f, 0.20f, 430.0f, 110.0f, 2.4f, 8.5f, 0.12f, 4, 24, 7, false, false, 1},
        {"NEGEV", "machine", GunKind::Primary, 38.0f, 800.0f, 0.14f, 560.0f, 420.0f, 4.6f, 6.5f, 0.07f, 150, 300, 1, true, false, 4},
        {"SSG-08", "silencer", GunKind::Primary, 88.0f, 48.0f, 0.04f, 790.0f, 740.0f, 1.7f, 11.0f, 0.13f, 10, 90, 1, false, false, 3},
        {"SCAR-20", "silencer", GunKind::Primary, 86.0f, 240.0f, 0.09f, 770.0f, 700.0f, 2.4f, 9.5f, 0.11f, 20, 90, 1, false, false, 4},
        {"F2000", "machine", GunKind::Primary, 34.0f, 850.0f, 0.07f, 600.0f, 350.0f, 2.2f, 4.2f, 0.04f, 30, 120, 1, true, false, 3},
        {"GROZA", "machine", GunKind::Primary, 44.0f, 680.0f, 0.08f, 590.0f, 350.0f, 2.3f, 6.2f, 0.06f, 30, 120, 1, true, false, 4},
        {"SCAR-H", "machine", GunKind::Primary, 46.0f, 580.0f, 0.07f, 600.0f, 380.0f, 2.4f, 6.4f, 0.06f, 20, 100, 1, true, false, 4},
        {"HONEY", "machine", GunKind::Primary, 40.0f, 780.0f, 0.05f, 620.0f, 370.0f, 2.0f, 5.0f, 0.04f, 30, 150, 1, true, false, 5},
        {"M14", "machine", GunKind::Primary, 52.0f, 400.0f, 0.06f, 640.0f, 400.0f, 2.6f, 7.0f, 0.08f, 20, 80, 1, false, false, 3},
        {"AA-12", "gun", GunKind::Primary, 18.0f, 300.0f, 0.16f, 470.0f, 160.0f, 3.4f, 9.0f, 0.12f, 8, 40, 6, true, false, 5},
        {"M60", "machine", GunKind::Primary, 42.0f, 550.0f, 0.13f, 560.0f, 420.0f, 5.0f, 7.5f, 0.08f, 100, 200, 1, true, false, 5},
    };
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(WeaponId::Count)) {
        return kDefs[0];
    }
    return kDefs[i];
}

inline float weaponCooldown(const WeaponDef& w) { return 60.0f / w.rpm; }

inline bool weaponIsGrenade(WeaponId id) { return weaponDef(id).kind == GunKind::Grenade; }

inline bool weaponIsLoadout(WeaponId id) {
    const WeaponDef& w = weaponDef(id);
    return w.kind != GunKind::Grenade;
}

inline int grenadeIndex(WeaponId id) {
    if (id == WeaponId::Flashbang) {
        return 0;
    }
    if (id == WeaponId::HEGrenade) {
        return 1;
    }
    if (id == WeaponId::SmokeGrenade) {
        return 2;
    }
    return -1;
}

inline WeaponId grenadeFromIndex(int i) {
    if (i == 1) {
        return WeaponId::HEGrenade;
    }
    if (i == 2) {
        return WeaponId::SmokeGrenade;
    }
    return WeaponId::Flashbang;
}

inline int weaponPrice(WeaponId id) {
    const WeaponDef& w = weaponDef(id);
    if (w.kind == GunKind::Grenade) {
        static const int kNade[] = {50, 80, 60};
        const int i = grenadeIndex(id);
        return kNade[i < 0 ? 0 : i];
    }
    if (w.melee) {
        return 0;
    }
    static const int kRarityPrice[] = {0, 150, 350, 650, 1100, 1800};
    const int r = w.rarity < 1 ? 1 : (w.rarity > 5 ? 5 : w.rarity);
    return kRarityPrice[r];
}

inline float rarityDamage(WeaponId id) {
    const WeaponDef& w = weaponDef(id);
    if (w.kind == GunKind::Grenade || w.melee) {
        return w.damage;
    }
    return w.damage * (0.72f + 0.10f * static_cast<float>(w.rarity));
}

inline std::string weaponHandSprite(WeaponId id) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "cs_gun_%d", static_cast<int>(id));
    return buf;
}

inline std::string weaponGroundSprite(WeaponId id) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "cs_ground_%d", static_cast<int>(id));
    return buf;
}
