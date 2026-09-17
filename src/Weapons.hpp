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
};

inline const WeaponDef& weaponDef(WeaponId id) {
    // Stats from original CSPSP guns.txt (id/delay/clip/type), tuned to 100 HP.
    static const WeaponDef kDefs[] = {
        {"KNIFE", "hold", GunKind::Knife, 40.0f, 200.0f, 0.0f, 0.0f, 22.0f, 0.2f, 1.5f, 0.0f, 1, 1, 1, false, true},
        {"GLOCK", "gun", GunKind::Secondary, 22.0f, 600.0f, 0.07f, 520.0f, 280.0f, 2.1f, 3.5f, 0.04f, 20, 80, 1, false, false},
        {"USP", "gun", GunKind::Secondary, 30.0f, 600.0f, 0.05f, 520.0f, 290.0f, 1.9f, 4.0f, 0.04f, 12, 72, 1, false, false},
        {"P228", "gun", GunKind::Secondary, 36.0f, 600.0f, 0.07f, 520.0f, 290.0f, 2.0f, 4.5f, 0.05f, 13, 65, 1, false, false},
        {"DEAGLE", "gun", GunKind::Secondary, 54.0f, 600.0f, 0.09f, 520.0f, 320.0f, 1.6f, 7.5f, 0.10f, 7, 42, 1, false, false},
        {"FIVESEVEN", "gun", GunKind::Secondary, 30.0f, 600.0f, 0.03f, 520.0f, 290.0f, 2.2f, 3.5f, 0.03f, 20, 120, 1, false, false},
        {"ELITE", "gun", GunKind::Secondary, 30.0f, 600.0f, 0.07f, 520.0f, 280.0f, 3.8f, 4.0f, 0.05f, 30, 150, 1, false, false},
        {"M3", "gun", GunKind::Primary, 16.0f, 55.0f, 0.16f, 480.0f, 150.0f, 4.0f, 8.0f, 0.10f, 8, 40, 6, false, false},
        {"XM1014", "gun", GunKind::Primary, 16.0f, 240.0f, 0.14f, 480.0f, 160.0f, 2.5f, 8.0f, 0.10f, 7, 35, 6, false, false},
        {"TMP", "silencer", GunKind::Primary, 22.0f, 706.0f, 0.12f, 500.0f, 240.0f, 1.6f, 3.0f, 0.04f, 30, 150, 1, true, false},
        {"MAC-10", "silencer", GunKind::Primary, 22.0f, 706.0f, 0.12f, 500.0f, 240.0f, 1.85f, 3.2f, 0.04f, 30, 120, 1, true, false},
        {"MP5", "silencer", GunKind::Primary, 26.0f, 667.0f, 0.09f, 500.0f, 260.0f, 1.7f, 3.0f, 0.03f, 30, 150, 1, true, false},
        {"UMP", "silencer", GunKind::Primary, 32.0f, 600.0f, 0.08f, 500.0f, 270.0f, 1.9f, 3.5f, 0.04f, 25, 125, 1, true, false},
        {"P90", "silencer", GunKind::Primary, 24.0f, 857.0f, 0.09f, 500.0f, 260.0f, 1.9f, 3.2f, 0.03f, 50, 150, 1, true, false},
        {"FAMAS", "machine", GunKind::Primary, 36.0f, 923.0f, 0.09f, 580.0f, 340.0f, 2.0f, 4.5f, 0.04f, 25, 100, 1, true, false},
        {"GALIL", "machine", GunKind::Primary, 38.0f, 667.0f, 0.09f, 580.0f, 340.0f, 1.9f, 5.0f, 0.05f, 35, 140, 1, true, false},
        {"SCOUT", "silencer", GunKind::Primary, 80.0f, 50.0f, 0.05f, 780.0f, 720.0f, 1.5f, 10.0f, 0.12f, 10, 100, 1, false, false},
        {"M4A1", "machine", GunKind::Primary, 36.0f, 600.0f, 0.06f, 600.0f, 360.0f, 2.1f, 4.5f, 0.04f, 30, 120, 1, true, false},
        {"AK-47", "machine", GunKind::Primary, 42.0f, 600.0f, 0.08f, 580.0f, 360.0f, 2.1f, 6.0f, 0.06f, 30, 120, 1, true, false},
        {"AUG", "machine", GunKind::Primary, 36.0f, 632.0f, 0.07f, 600.0f, 380.0f, 2.5f, 4.5f, 0.04f, 30, 120, 1, true, false},
        {"SG552", "machine", GunKind::Primary, 36.0f, 632.0f, 0.07f, 600.0f, 370.0f, 2.4f, 4.8f, 0.05f, 30, 120, 1, true, false},
        {"SG550", "silencer", GunKind::Primary, 80.0f, 240.0f, 0.10f, 760.0f, 680.0f, 2.2f, 9.0f, 0.10f, 30, 120, 1, false, false},
        {"G3SG1", "silencer", GunKind::Primary, 80.0f, 200.0f, 0.10f, 760.0f, 680.0f, 3.2f, 9.0f, 0.10f, 20, 100, 1, false, false},
        {"AWP", "silencer", GunKind::Primary, 110.0f, 43.0f, 0.02f, 800.0f, 760.0f, 2.3f, 14.0f, 0.16f, 10, 40, 1, false, false},
        {"M249", "machine", GunKind::Primary, 36.0f, 632.0f, 0.12f, 560.0f, 400.0f, 4.2f, 6.0f, 0.06f, 100, 200, 1, true, false},
        {"FLASHBANG", "hold", GunKind::Grenade, 0.0f, 60.0f, 0.0f, 180.0f, 200.0f, 1.0f, 2.0f, 0.0f, 1, 1, 1, false, false},
        {"HE-GRENADE", "hold", GunKind::Grenade, 100.0f, 60.0f, 0.0f, 200.0f, 210.0f, 1.0f, 8.0f, 0.0f, 1, 1, 1, false, false},
        {"SMOKE", "hold", GunKind::Grenade, 0.0f, 60.0f, 0.0f, 160.0f, 190.0f, 1.0f, 2.0f, 0.0f, 1, 1, 1, false, false},
    };
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(WeaponId::Count)) {
        return kDefs[0];
    }
    return kDefs[i];
}

inline float weaponCooldown(const WeaponDef& w) { return 60.0f / w.rpm; }

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
