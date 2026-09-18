#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
const char* kTSkins[] = {"manBrown", "hitman1", "survivor1", "manOld"};
const char* kCTSkins[] = {"manBlue", "soldier1", "robot1", "womanGreen"};

const char* actorSkin(Team team, int id) {
    const int t = teamIndex(team);
    if (t == 0) {
        return kTSkins[id % 4];
    }
    if (t == 1) {
        return kCTSkins[id % 4];
    }
    static const char* kExtra[] = {"robot1", "womanGreen", "survivor1", "manOld"};
    return kExtra[(t + id) % 4];
}

Team botLoadoutTeam(Team team) {
    return (teamIndex(team) % 2) == 0 ? Team::Terrorist : Team::Counter;
}

Team fillTeam(GameMode mode, int teamCount, Team localTeam, int i) {
    (void)localTeam;
    if (mode == GameMode::LastSurvivor || mode == GameMode::SoloVsAll) {
        return Team::Terrorist;
    }
    const int n = std::max(2, teamCount);
    return teamFromIndex(i % n);
}
constexpr float kRadarPing = 3.2f;
constexpr float kBaseHitR = 22.0f;
constexpr int kBaseMaxHp = 6000;
constexpr int kNukeHits = 4;

SDL_Color teamColor(Team t) {
    static const SDL_Color kCols[kMaxTeams] = {
        {230, 140, 40, 255}, {70, 140, 230, 255}, {210, 70, 90, 255},
        {200, 90, 210, 255}, {200, 200, 210, 255}, {50, 210, 210, 255}
    };
    return kCols[teamIndex(t)];
}

SDL_Color baseHudColor(Team t, Team localTeam) {
    if (t == localTeam) {
        return {255, 220, 70, 255};
    }
    return teamColor(t);
}

float frand(float a, float b) {
    return a + (b - a) * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
}

void drawSkull(SDL_Renderer* r, int x, int y, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect cr{x, y, 8, 7};
    SDL_RenderFillRect(r, &cr);
    SDL_Rect jaw{x + 2, y + 6, 4, 2};
    SDL_RenderFillRect(r, &jaw);
    SDL_SetRenderDrawColor(r, 20, 16, 16, 255);
    SDL_Rect e1{x + 1, y + 2, 2, 2};
    SDL_Rect e2{x + 5, y + 2, 2, 2};
    SDL_RenderFillRect(r, &e1);
    SDL_RenderFillRect(r, &e2);
    SDL_Rect nose{x + 3, y + 4, 2, 2};
    SDL_RenderFillRect(r, &nose);
}

int irand(int n) { return n <= 0 ? 0 : std::rand() % n; }

constexpr int kJobHunt = 0;
constexpr int kJobFollow = 1;
constexpr int kJobSquad = 2;
constexpr int kJobPatrol = 3;

struct DiffTune {
    int bots;
    float botSpeed;
    float spreadMul;
    float reaction;
    float rangeMul;
    float fireChance;
    float botHp;
    float incomingMul;
    float botDmgMul;
    float respawn;
    float shotDelay;
};

DiffTune tune(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:
            return {5, 0.50f, 3.2f, 1.15f, 0.48f, 0.12f, 55.0f, 0.48f, 0.50f, 4.4f, 2.20f};
        case Difficulty::Hard:
            return {13, 0.98f, 0.72f, 0.22f, 0.85f, 0.35f, 100.0f, 0.95f, 0.88f, 1.80f, 1.45f};
        default:
            return {7, 0.80f, 1.55f, 0.48f, 0.70f, 0.22f, 80.0f, 0.70f, 0.68f, 3.0f, 1.75f};
    }
}

struct PlayerPose {
    float bodyA = 0;
    float rarmA = 0;
    float rhandA = 0;
    float larmA = 0;
    float lhandA = 0;
    float gunA = 0;
};

// animations.txt: duration BODY RIGHTARM RIGHTHAND LEFTARM LEFTHAND GUN (degrees)
PlayerPose poseFor(const Actor& a) {
    const WeaponDef& w = weaponDef(a.weapon);
    PlayerPose p;
    if (w.kind == GunKind::Knife) {
        if (a.muzzleT > 0.0f) {
            p = {rad(-15), rad(-30), rad(-50), rad(-25), rad(10), rad(-110)};
        } else {
            p = {0.0f, rad(20), rad(-40), rad(-10), rad(10), rad(-100)};
        }
    } else if (w.kind == GunKind::Secondary) {
        if (a.muzzleT > 0.0f) {
            p = {rad(15), rad(-10), rad(-40), rad(10), rad(60), 0.0f};
        } else {
            p = {rad(5), rad(-10), rad(-40), rad(20), rad(50), 0.0f};
        }
    } else if (w.kind == GunKind::Grenade) {
        p = {0.0f, rad(20), rad(-30), rad(-20), rad(30), rad(60)};
    } else if (a.reloadT > 0.0f) {
        p = {rad(5), rad(25), rad(-50), rad(-20), rad(30), rad(-30)};
    } else if (a.muzzleT > 0.0f) {
        p = {rad(40), rad(25), rad(-60), 0.0f, rad(65), 0.0f};
    } else {
        p = {rad(30), rad(15), rad(-60), rad(10), rad(50), 0.0f};
    }
    return p;
}

Vec2 gunHoldWorld(const Actor& a) {
    const PlayerPose p = poseFor(a);
    const float facing = a.angle;
    const float mRot = facing - kPi * 0.5f;
    const float rotation = facing;
    const float cx = a.pos.x - 5.0f * std::cos(rotation);
    const float cy = a.pos.y - 5.0f * std::sin(rotation);
    const float dx = 10.0f * std::cos(mRot + p.bodyA);
    const float dy = 10.0f * std::sin(mRot + p.bodyA);
    float x = cx - dx;
    float y = cy - dy;
    x += 8.0f * std::cos(rotation + p.rarmA);
    y += 8.0f * std::sin(rotation + p.rarmA);
    x += 10.0f * std::cos(rotation + p.rhandA);
    y += 10.0f * std::sin(rotation + p.rhandA);
    return {x, y};
}

WeaponId botWeapon(Difficulty d, Team team, int id) {
    if (d == Difficulty::Easy) {
        static const WeaponId kEasy[] = {WeaponId::Glock, WeaponId::USP, WeaponId::MP5, WeaponId::TMP};
        return kEasy[id % 4];
    }
    if (d == Difficulty::Hard) {
        static const WeaponId kHardT[] = {WeaponId::AK47, WeaponId::GALIL, WeaponId::SG552,
                                          WeaponId::Deagle, WeaponId::XM1014, WeaponId::AWP};
        static const WeaponId kHardCT[] = {WeaponId::M4A1, WeaponId::FAMAS, WeaponId::AUG,
                                           WeaponId::MP5, WeaponId::M249, WeaponId::Scout};
        return team == Team::Terrorist ? kHardT[id % 6] : kHardCT[id % 6];
    }
    if (team == Team::Terrorist) {
        return (id % 2) ? WeaponId::AK47 : WeaponId::GALIL;
    }
    return (id % 2) ? WeaponId::M4A1 : WeaponId::FAMAS;
}

void drawCspspPlayer(SDL_Renderer* r, Assets& assets, const Actor& a, float sx, float sy, float z) {
    int sid = a.skinId;
    if (sid < 0 || sid >= kSkinCount) {
        sid = 0;
    }
    char pre[24];
    std::snprintf(pre, sizeof(pre), "cs_p_%d%d", sid / 4, sid % 4);
    const std::string base(pre);
    const WeaponDef& w = weaponDef(a.weapon);
    const PlayerPose p = poseFor(a);

    const float facing = a.angle;
    const float mRot = facing - kPi * 0.5f;
    const float rotation = facing;

    const bool moving = a.vel.length2() > 25.0f;
    const float walkAng = moving ? angleOf(a.vel) : facing;
    const float walkScaleY = moving ? std::sin((a.walkT / 0.16f) * kPi * 0.5f) : 0.42f;
    const bool walkFlip = moving && a.walkT > 0.16f;

    float x = sx - 4.0f * std::cos(rotation) * z;
    float y = sy - 4.0f * std::sin(rotation) * z;
    assets.drawHotspotXY(r, base + "_legs", x, y, deg(walkAng - kPi * 0.5f), z, walkScaleY * z, 16.0f, 16.0f,
                         walkFlip);

    x = sx - 5.0f * std::cos(rotation) * z;
    y = sy - 5.0f * std::sin(rotation) * z;
    const float centerx = x;
    const float centery = y;
    const float dx = 10.0f * std::cos(mRot + p.bodyA) * z;
    const float dy = 10.0f * std::sin(mRot + p.bodyA) * z;
    assets.drawHotspot(r, base + "_body", x, y, deg(mRot + p.bodyA), z, 16.0f, 8.0f, false);

    x = centerx + dx;
    y = centery + dy;
    assets.drawHotspot(r, base + "_arm", x, y, deg(mRot + p.larmA), z, 4.0f, 4.0f, true);
    x += 8.0f * std::cos(rotation + p.larmA) * z;
    y += 8.0f * std::sin(rotation + p.larmA) * z;
    assets.drawHotspot(r, base + "_hand", x, y, deg(mRot + p.lhandA), z, 4.0f, 3.0f, true);

    x = centerx - dx;
    y = centery - dy;
    assets.drawHotspot(r, base + "_arm", x, y, deg(mRot + p.rarmA), z, 4.0f, 4.0f, false);
    x += 8.0f * std::cos(rotation + p.rarmA) * z;
    y += 8.0f * std::sin(rotation + p.rarmA) * z;
    assets.drawHotspot(r, base + "_hand", x, y, deg(mRot + p.rhandA), z, 4.0f, 3.0f, false);
    x += 10.0f * std::cos(rotation + p.rhandA) * z;
    y += 10.0f * std::sin(rotation + p.rhandA) * z;
    assets.drawHotspot(r, weaponHandSprite(a.weapon), x, y, deg(mRot + p.gunA), z, 16.0f, 8.0f, false);

    if (a.muzzleT > 0.0f && !w.melee && w.kind != GunKind::Grenade) {
        assets.drawHotspot(r, "cs_muzzle_0", x, y, deg(facing - kPi * 0.5f), z, 16.0f, -16.0f, false);
    }
    assets.drawHotspot(r, base + "_head", centerx, centery, deg(mRot), z, 8.0f, 7.0f, false);
}

void drawBulletTracer(SDL_Renderer* r, const Camera& cam, const Bullet& b) {
    const float spd = b.vel.length();
    if (spd < 1.0f) {
        return;
    }
    const Vec2 dir = b.vel.normalized();
    const float unit = std::max(6.0f, spd * 0.018f);
    auto toS = [&](Vec2 w) { return cam.toScreen(w.x, w.y); };
    auto back = [&](float k) {
        Vec2 w{b.pos.x - dir.x * unit * k, b.pos.y - dir.y * unit * k};
        const float along = (w.x - b.origin.x) * dir.x + (w.y - b.origin.y) * dir.y;
        if (along < 0.0f) {
            w = b.origin;
        }
        return toS(w);
    };
    const Vec2 tip = toS(b.pos);
    auto line = [&](const Vec2& a, const Vec2& c) {
        SDL_RenderDrawLine(r, static_cast<int>(a.x), static_cast<int>(a.y), static_cast<int>(c.x),
                           static_cast<int>(c.y));
    };
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(r, 100, 100, 100, 80);
    line(back(4.0f), tip);
    SDL_SetRenderDrawColor(r, 100, 100, 100, 130);
    line(back(3.0f), tip);
    SDL_SetRenderDrawColor(r, 100, 100, 100, 220);
    line(back(1.0f), tip);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 240, 0, 80);
    line(back(3.0f), tip);
    SDL_SetRenderDrawColor(r, 255, 240, 0, 140);
    line(back(2.0f), tip);
    SDL_SetRenderDrawColor(r, 255, 240, 0, 230);
    line(back(1.0f), tip);
    SDL_SetRenderDrawColor(r, 255, 240, 0, 255);
    SDL_Rect pix{static_cast<int>(tip.x), static_cast<int>(tip.y) - 1, 2, 2};
    SDL_RenderFillRect(r, &pix);
}

Particle* takeParticle(std::vector<Particle>& ps) {
    for (auto& p : ps) {
        if (!p.active) {
            return &p;
        }
    }
    return ps.empty() ? nullptr : &ps[0];
}

void spawnParticle(std::vector<Particle>& ps, Vec2 pos, Vec2 vel, SDL_Color c, float life, float size) {
    Particle* p = takeParticle(ps);
    if (!p) {
        return;
    }
    p->active = true;
    p->pos = pos;
    p->vel = vel;
    p->color = c;
    p->colorEnd = c;
    p->colorEnd.a = 0;
    p->life = life;
    p->maxLife = life;
    p->size = size;
    p->sizeEnd = size;
    p->sprite = -1;
    p->angle = 0.0f;
    p->spin = 0.0f;
}

void spawnFxParticle(std::vector<Particle>& ps, Vec2 pos, Vec2 vel, int sprite, float life, float size,
                     float sizeEnd, SDL_Color start, SDL_Color end, float spin) {
    Particle* p = takeParticle(ps);
    if (!p) {
        return;
    }
    p->active = true;
    p->pos = pos;
    p->vel = vel;
    p->color = start;
    p->colorEnd = end;
    p->life = life;
    p->maxLife = life;
    p->size = size;
    p->sizeEnd = sizeEnd;
    p->sprite = sprite;
    p->angle = frand(0.0f, 360.0f);
    p->spin = spin;
}
} // namespace

void drawCspspSkin(SDL_Renderer* r, Assets& assets, int skinId, float sx, float sy, float z, float angle) {
    Actor a;
    a.skinId = skinId;
    a.weapon = WeaponId::AK47;
    a.angle = angle;
    a.alive = true;
    a.active = true;
    drawCspspPlayer(r, assets, a, sx, sy, z);
}

bool World::loadMap(const std::string& path) {
    freeMinimap();
    return map_.load(path);
}

void World::freeMinimap() {
    if (miniTex_) {
        SDL_DestroyTexture(miniTex_);
        miniTex_ = nullptr;
    }
    miniGenW_ = miniGenH_ = 0;
}

void World::bakeMinimap(SDL_Renderer* r) {
    if (miniTex_ && miniGenW_ == map_.width() && miniGenH_ == map_.height()) {
        return;
    }
    freeMinimap();
    constexpr int N = 64;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, N, N, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        return;
    }
    const Uint32 wall = SDL_MapRGBA(surf->format, 70, 70, 60, 255);
    const Uint32 floor = SDL_MapRGBA(surf->format, 10, 12, 10, 200);
    auto* pix = static_cast<Uint32*>(surf->pixels);
    const int mw = std::max(1, map_.width());
    const int mh = std::max(1, map_.height());
    for (int py = 0; py < N; ++py) {
        const int ty = py * mh / N;
        for (int px = 0; px < N; ++px) {
            const int tx = px * mw / N;
            pix[py * N + px] = map_.solid(tx, ty) ? wall : floor;
        }
    }
    miniTex_ = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    if (miniTex_) {
        SDL_SetTextureBlendMode(miniTex_, SDL_BLENDMODE_BLEND);
        miniGenW_ = map_.width();
        miniGenH_ = map_.height();
    }
}

bool World::nearHumanSpawn(const Vec2& p) const {
    const float minD = kTile * 3.25f;
    const float minD2 = minD * minD;
    const Actor* me = local();
    const Team t = me ? me->team : Team::Counter;
    for (const Vec2& s : map_.spawns(t)) {
        if ((p - s).length2() < minD2) {
            return true;
        }
    }
    if (me && me->active && (p - me->pos).length2() < minD2) {
        return true;
    }
    return false;
}

bool World::onVisibleScreen(const Vec2& p) const {
    const float pad = 56.0f;
    return p.x >= camView_.x - pad && p.y >= camView_.y - pad && p.x <= camView_.x + camViewW_ + pad &&
           p.y <= camView_.y + camViewH_ + pad;
}

Vec2 World::zombieEdgeSpawn(int id) {
    const int mw = map_.width();
    const int mh = map_.height();
    const int inset = 2;
    struct Box {
        int x0, y0, x1, y1;
    };
    const int xL = inset;
    const int xM0 = std::max(inset, mw / 3);
    const int xM1 = std::max(xM0 + 1, (mw * 2) / 3);
    const int xR = std::max(inset + 1, mw - inset);
    const int yT = inset;
    const int yBand = std::max(inset + 2, mh / 6);
    const int yMid0 = std::max(inset, mh / 3);
    const int yMid1 = std::max(yMid0 + 1, (mh * 2) / 3);
    const int yB0 = std::max(inset, (mh * 5) / 6);
    const int yB = std::max(inset + 1, mh - inset);
    const int xThin = std::max(inset + 2, mw / 6);
    const int xRight = std::max(inset, (mw * 5) / 6);

    const Box edges[] = {
        {xL, yT, xM0, yBand},
        {xM0, yT, xM1, yBand},
        {xM1, yT, xR, yBand},
        {xL, yMid0, xThin, yMid1},
        {xRight, yT, xR, yMid1},
        {xL, yB0, xM0, yB},
        {xM0, yB0, xM1, yB},
        {xM1, yB0, xR, yB},
    };
    const int nEdges = 8;
    const int start = (id + wave_ * 3 + irand(nEdges)) % nEdges;
    Vec2 hidden[32];
    Vec2 visible[32];
    int nHidden = 0;
    int nVisible = 0;

    auto consider = [&](const Vec2& c) {
        if (c.length2() < 1.0f || nearHumanSpawn(c)) {
            return;
        }
        if (!onVisibleScreen(c)) {
            if (nHidden < 32) {
                hidden[nHidden++] = c;
            }
        } else if (nVisible < 32) {
            visible[nVisible++] = c;
        }
    };

    auto sampleBox = [&](const Box& b) {
        int x0 = std::max(inset, std::min(b.x0, mw - inset - 1));
        int y0 = std::max(inset, std::min(b.y0, mh - inset - 1));
        int x1 = std::max(x0 + 1, std::min(b.x1, mw - inset));
        int y1 = std::max(y0 + 1, std::min(b.y1, mh - inset));
        for (int t = 0; t < 28; ++t) {
            const int tx = x0 + irand(x1 - x0);
            const int ty = y0 + irand(y1 - y0);
            if (!tileOpen(tx, ty)) {
                continue;
            }
            const Vec2 c = tileCenter(tx, ty);
            if (!map_.solidWorld(c.x, c.y, kPlayerRadius)) {
                consider(c);
            }
        }
    };

    for (int i = 0; i < nEdges; ++i) {
        sampleBox(edges[(start + i) % nEdges]);
        if (nHidden >= 8) {
            break;
        }
    }
    if (nHidden > 0) {
        return hidden[irand(nHidden)];
    }

    Vec2 best = map_.spawn(Team::Terrorist, id);
    float bestD = -1.0f;
    const Vec2 mid{camView_.x + camViewW_ * 0.5f, camView_.y + camViewH_ * 0.5f};
    for (int i = 0; i < nVisible; ++i) {
        const float d = (visible[i] - mid).length2();
        if (d > bestD) {
            bestD = d;
            best = visible[i];
        }
    }
    return best;
}

void World::rollTeamSkins(int localTeamIdx, int localSkin) {
    localSkin_ = ((localSkin % kSkinCount) + kSkinCount) % kSkinCount;
    for (int i = 0; i < kMaxTeams; ++i) {
        teamSkin_[i] = 0;
    }
    if (!modeUsesTeams(mode_) && mode_ != GameMode::Zombie) {
        return;
    }
    bool used[kSkinCount]{};
    const int lt = mode_ == GameMode::Zombie ? teamIndex(Team::Counter) : localTeamIdx;
    teamSkin_[lt] = localSkin_;
    used[localSkin_] = true;
    if (mode_ == GameMode::Zombie) {
        return;
    }
    for (int i = 0; i < teamCount_; ++i) {
        if (i == lt) {
            continue;
        }
        int pool[kSkinCount];
        int np = 0;
        for (int s = 0; s < kSkinCount; ++s) {
            if (!used[s]) {
                pool[np++] = s;
            }
        }
        const int pick = np > 0 ? pool[irand(np)] : irand(kSkinCount);
        teamSkin_[i] = pick;
        used[pick] = true;
    }
}

int World::takeSkin(Team team, bool zombie) {
    if (zombie) {
        return 0;
    }
    if (modeUsesTeams(mode_) || mode_ == GameMode::Zombie) {
        return teamSkin_[teamIndex(team)];
    }
    int counts[kSkinCount]{};
    for (const auto& a : actors_) {
        if (!a.active || a.zombie) {
            continue;
        }
        if (a.skinId >= 0 && a.skinId < kSkinCount) {
            counts[a.skinId]++;
        }
    }
    int best = 0;
    int bestC = 999;
    const int start = irand(kSkinCount);
    for (int k = 0; k < kSkinCount; ++k) {
        const int s = (start + k) % kSkinCount;
        if (counts[s] < bestC) {
            bestC = counts[s];
            best = s;
        }
    }
    return best;
}

void World::spawnActor(int id, Team team, bool bot, const std::string& name, bool zombie) {
    Actor& a = actors_[id];
    a = Actor{};
    a.id = id;
    a.alive = true;
    a.bot = bot;
    a.team = team;
    a.name = name;
    a.zombie = zombie;
    a.skinId = takeSkin(team, zombie);
    a.active = true;
    a.walkT = 0.0f;
    a.flashT = 0.0f;
    a.flashIntensity = 1.0f;
    a.skin = zombie ? "zoimbie1" : actorSkin(team, id);
    a.weapon = zombie ? WeaponId::Knife
                      : (a.bot ? botWeapon(difficulty_, botLoadoutTeam(team), id)
                               : ((teamIndex(team) % 2) == 0 ? WeaponId::AK47 : WeaponId::M4A1));
    const WeaponDef& w = weaponDef(a.weapon);
    a.mag = w.mag;
    a.reserve = w.reserve;
    a.hp = zombie ? (38 + wave_ * 6) : (a.bot ? static_cast<int>(tune(difficulty_).botHp) : 100);
    a.radarT = 0.0f;
    const int spawnN = modeUsesTeams(mode_) ? teamCount_ : 2;
    const Team spawnSide = modeUsesTeams(mode_) ? team : ((id % 2) ? Team::Counter : Team::Terrorist);
    a.pos = zombie ? zombieEdgeSpawn(id)
                   : (mode_ == GameMode::ProtectBase ? spawnAtBase(team, id) : map_.spawn(spawnSide, id, spawnN));
    unstickActor(a);
    a.angle = (kPi * 2.0f * static_cast<float>(teamIndex(spawnSide))) / static_cast<float>(std::max(2, spawnN));
    a.botSteer = {std::cos(a.angle), std::sin(a.angle)};
    a.botLast = a.pos;
    a.botLockT = 0.0f;
    a.botStuckT = 0.0f;
    a.botPathT = 0.0f;
    a.botGoalD = 1.0e9f;
    a.botWayX = a.botWayY = -1;
    a.botPrevX = a.botPrevY = -1;
    a.botWallDir = (id % 2 == 0) ? 1 : -1;
    a.botFail = 0;
    a.botJob = zombie ? ((id % 2 == 0) ? kJobHunt : kJobPatrol) : kJobHunt;
    a.botBuddy = -1;
    a.botJobT = 4.0f + static_cast<float>(id % 7);
    a.botHome = a.pos;
    if (a.botJob == kJobPatrol) {
        a.botHome = pickPatrolPoint(a);
    }
}

void World::startMatch(Team localTeam, int botCount, int localId, Difficulty difficulty, GameMode mode, int teamCount,
                       int localSkin) {
    difficulty_ = difficulty;
    mode_ = mode;
    if (modeUsesTeams(mode_)) {
        teamCount_ = std::max(2, std::min(kMaxTeams, teamCount));
    } else if (mode_ == GameMode::Zombie) {
        teamCount_ = 2;
    } else {
        teamCount_ = 1;
    }
    localId_ = localId;
    wave_ = 0;
    wavePause_ = 0.0f;
    matchOver_ = false;
    winnerId_ = -1;
    banner_[0] = 0;
    bannerT_ = 0.0f;
    bullets_.assign(64, Bullet{});
    particles_.assign(400, Particle{});
    loot_.assign(24, Loot{});
    blasts_.assign(8, BlastFx{});
    smokes_.assign(6, SmokeCloud{});
    scorches_.assign(12, Scorch{});
    for (auto& a : actors_) {
        a.active = false;
    }
    setupBases();
    scanNukes();
    if (mode_ == GameMode::Zombie) {
        localTeam = Team::Counter;
    }
    if (localSkin < 0 || localSkin >= kSkinCount) {
        localSkin = irand(kSkinCount);
    }
    rollTeamSkins(teamIndex(localTeam), localSkin);
    spawnActor(localId_, localTeam, false, "YOU", false);
    int next = 0;
    auto takeSlot = [&]() {
        while (next == localId_ && next < kMaxPlayers) {
            ++next;
        }
        return next < kMaxPlayers ? next++ : -1;
    };
    if (mode_ == GameMode::Zombie) {
        const int allies = std::max(1, botCount / 3);
        for (int i = 0; i < allies; ++i) {
            const int id = takeSlot();
            if (id < 0) {
                break;
            }
            char n[16];
            std::snprintf(n, sizeof(n), "ALLY%02d", i + 1);
            spawnActor(id, localTeam, true, n, false);
        }
        wavePause_ = 1.2f;
        wave_ = 0;
    } else {
        for (int i = 0; i < botCount; ++i) {
            const int id = takeSlot();
            if (id < 0) {
                break;
            }
            const Team team = fillTeam(mode_, teamCount_, localTeam, i);
            char n[16];
            std::snprintf(n, sizeof(n), "BOT%02d", i + 1);
            spawnActor(id, team, true, n, false);
        }
    }
    assignBotJobs();
    paused_ = false;
}

int World::findSlot() const {
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (!actors_[i].active) {
            return i;
        }
    }
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (actors_[i].zombie && !actors_[i].alive) {
            return i;
        }
    }
    return -1;
}

void World::spawnWave(int n) {
    wave_ = n;
    int humans = 0;
    for (auto& a : actors_) {
        if (a.active && !a.zombie) {
            ++humans;
        }
    }
    const int room = std::max(4, kMaxPlayers - std::max(1, humans));
    int base = 4 + n * 2;
    int cap = 14;
    if (map_.width() >= 100) {
        base = 6 + n * 3;
        cap = 20;
    }
    if (map_.width() >= 200) {
        base = 8 + n * 4;
        cap = 26;
    }
    if (map_.width() >= 400) {
        base = 10 + n * 5;
        cap = room;
    }
    const int want = std::min(std::min(base, cap), room);
    for (int i = 0; i < want; ++i) {
        const int id = findSlot();
        if (id < 0) {
            break;
        }
        char nm[16];
        std::snprintf(nm, sizeof(nm), "Z%02d", i + 1);
        spawnActor(id, Team::Terrorist, true, nm, true);
        if (auto* me = local()) {
            actors_[id].angle = angleOf(me->pos - actors_[id].pos);
        }
    }
}

void World::updateWaves(float dt) {
    if (mode_ != GameMode::Zombie || matchOver_) {
        return;
    }
    if (wavePause_ > 0.0f) {
        wavePause_ -= dt;
        if (wavePause_ <= 0.0f) {
            spawnWave(wave_ + 1);
        }
        return;
    }
    int living = 0;
    for (auto& a : actors_) {
        if (a.active && a.zombie && a.alive) {
            ++living;
        }
    }
    if (living == 0 && wave_ > 0) {
        wavePause_ = 2.4f;
    }
}

int World::aliveCount() const {
    int n = 0;
    for (auto& a : actors_) {
        if (a.active && a.alive) {
            ++n;
        }
    }
    return n;
}

bool World::hostile(const Actor& a, const Actor& b) const {
    if (a.id == b.id) {
        return false;
    }
    if (mode_ == GameMode::LastSurvivor) {
        return true;
    }
    if (mode_ == GameMode::SoloVsAll) {
        return a.bot != b.bot;
    }
    return a.team != b.team;
}

void World::checkWinner() {
    if (matchOver_) {
        return;
    }
    if (mode_ == GameMode::ProtectBase) {
        int aliveBases = 0;
        Team last = Team::Terrorist;
        for (int i = 0; i < teamCount_; ++i) {
            if (bases_[i].active && bases_[i].hp > 0) {
                ++aliveBases;
                last = bases_[i].team;
            }
        }
        if (aliveBases <= 1) {
            matchOver_ = true;
            paused_ = true;
            winnerId_ = -1;
            if (const Actor* me = local()) {
                if (me->active && (aliveBases == 0 || me->team == last)) {
                    winnerId_ = localId_;
                }
            }
            if (winnerId_ < 0) {
                for (const auto& a : actors_) {
                    if (a.active && a.team == last) {
                        winnerId_ = a.id;
                        break;
                    }
                }
            }
        }
        return;
    }
    if (mode_ != GameMode::LastSurvivor && mode_ != GameMode::SoloVsAll) {
        return;
    }
    if (mode_ == GameMode::SoloVsAll) {
        Actor* me = local();
        if (me && !me->alive) {
            matchOver_ = true;
            winnerId_ = -1;
            paused_ = true;
            return;
        }
        bool anyEnemy = false;
        for (auto& a : actors_) {
            if (a.active && a.alive && me && hostile(*me, a)) {
                anyEnemy = true;
                break;
            }
        }
        if (!anyEnemy) {
            matchOver_ = true;
            winnerId_ = localId_;
            paused_ = true;
        }
        return;
    }
    int living = 0;
    int last = -1;
    for (auto& a : actors_) {
        if (a.active && a.alive) {
            ++living;
            last = a.id;
        }
    }
    if (living <= 1) {
        matchOver_ = true;
        winnerId_ = last;
        paused_ = true;
    }
}

void World::addPlayer(int id, Team team, bool bot, const std::string& name) {
    if (id < 0 || id >= kMaxPlayers) {
        return;
    }
    spawnActor(id, team, bot, name);
}

void World::killActor(Actor& a) {
    if (!a.alive) {
        return;
    }
    a.alive = false;
    a.hp = 0;
    a.deaths++;
    a.radarT = 0.0f;
    a.botThink = 0.0f;
    dropLoot(a.pos, a.weapon, a.mag + a.reserve);
}

void World::dropLoot(Vec2 pos, WeaponId weapon, int ammo) {
    if (weapon == WeaponId::Knife || weaponDef(weapon).melee) {
        return;
    }
    int slot = -1;
    for (int i = 0; i < static_cast<int>(loot_.size()); ++i) {
        if (!loot_[static_cast<size_t>(i)].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (loot_.empty()) {
            return;
        }
        slot = static_cast<int>(tick_ % loot_.size());
    }
    Loot& l = loot_[static_cast<size_t>(slot)];
    l.active = true;
    l.pos = pos;
    l.weapon = weapon;
    l.ammo = std::max(ammo, weaponDef(weapon).mag);
}

void World::applyLoot(Actor& a, Loot& loot) {
    if (!loot.active || !a.alive) {
        return;
    }
    const WeaponDef& w = weaponDef(loot.weapon);
    const int give = std::max(w.mag, loot.ammo);
    if (a.weapon == loot.weapon) {
        a.reserve += give;
    } else {
        a.weapon = loot.weapon;
        a.mag = w.mag;
        a.reserve += give;
    }
    a.reserve = std::min(a.reserve, w.reserve * 4);
    loot.active = false;
}

void World::updateLoot() {
    for (auto& a : actors_) {
        if (!a.active || !a.alive || a.bot || a.zombie) {
            continue;
        }
        for (auto& l : loot_) {
            if (!l.active) {
                continue;
            }
            if ((a.pos - l.pos).length2() <= 24.0f * 24.0f) {
                applyLoot(a, l);
            }
        }
    }
}

void World::fillBots(int count) {
    if (count <= 0 || mode_ == GameMode::Zombie) {
        return;
    }
    const Team localTeam = (localId_ >= 0 && localId_ < kMaxPlayers && actors_[localId_].active)
                               ? actors_[localId_].team
                               : Team::Counter;
    for (int i = 0; i < count; ++i) {
        const int id = findSlot();
        if (id < 0) {
            break;
        }
        const Team team = fillTeam(mode_, teamCount_, localTeam, i);
        char n[16];
        std::snprintf(n, sizeof(n), "BOT%02d", i + 1);
        spawnActor(id, team, true, n, false);
    }
    assignBotJobs();
}

void World::respawn(Actor& a) {
    a.alive = true;
    a.hp = 100;
    const int spawnN = modeUsesTeams(mode_) ? teamCount_ : 2;
    const Team spawnSide = modeUsesTeams(mode_) ? a.team : ((a.id % 2) ? Team::Counter : Team::Terrorist);
    a.pos = mode_ == GameMode::ProtectBase ? spawnAtBase(a.team, a.id) : map_.spawn(spawnSide, a.id, spawnN);
    a.vel = {};
    a.radarT = 0.0f;
    a.flashT = 0.0f;
    a.flashIntensity = 1.0f;
    a.botThink = 0.0f;
    a.botLockT = 0.0f;
    a.botStuckT = 0.0f;
    a.botPathT = 0.0f;
    a.botGoalD = 1.0e9f;
    a.botLast = a.pos;
    a.botWayX = a.botWayY = -1;
    a.botPrevX = a.botPrevY = -1;
    a.botFail = 0;
    a.botSteer = {std::cos(a.angle), std::sin(a.angle)};
    a.hp = a.bot ? static_cast<int>(tune(difficulty_).botHp) : 100;
    const WeaponDef& w = weaponDef(a.weapon);
    if (a.mag <= 0) {
        a.mag = w.mag;
    }
}

void World::applyAnalog(Actor& a, float ax, float ay, float dt, bool faceMove) {
    const Vec2 stick{ax, ay};
    const float mag = clamp(stick.length(), 0.0f, 1.0f);
    const float speedMul = a.bot ? tune(difficulty_).botSpeed * (a.zombie ? 0.70f + wave_ * 0.015f : 1.0f) : 1.0f;
    if (mag > 0.08f) {
        const Vec2 dir = stick.normalized();
        if (faceMove) {
            a.angle = angleOf(dir);
        }
        a.vel = dir * (kPlayerSpeed * speedMul * mag);
    } else {
        a.vel = {0.0f, 0.0f};
    }
    (void)dt;
}

void World::moveActor(Actor& a, float dt) {
    if (map_.solidWorld(a.pos.x, a.pos.y, kPlayerRadius * 0.9f)) {
        unstickActor(a);
    }

    const Vec2 next = a.pos + a.vel * dt;
    const bool xOk = !map_.solidWorld(next.x, a.pos.y, kPlayerRadius);
    const bool yOk = !map_.solidWorld(a.pos.x, next.y, kPlayerRadius);
    if (xOk) {
        a.pos.x = next.x;
    }
    if (yOk) {
        a.pos.y = next.y;
    }
    if (!xOk && !yOk) {
        a.vel = {0.0f, 0.0f};
    } else if (!xOk) {
        a.vel.x = 0.0f;
    } else if (!yOk) {
        a.vel.y = 0.0f;
    }
    a.pos.x = clamp(a.pos.x, kTile + 4.0f, map_.pixelW() - kTile - 4.0f);
    a.pos.y = clamp(a.pos.y, kTile + 4.0f, map_.pixelH() - kTile - 4.0f);
    if (a.vel.length2() > 25.0f) {
        a.walkT += dt;
        if (a.walkT > 0.32f) {
            a.walkT -= 0.32f;
        }
    }
    if (map_.solidWorld(a.pos.x, a.pos.y, kPlayerRadius * 0.9f)) {
        unstickActor(a);
    }
}

void World::unstickActor(Actor& a) {
    if (!map_.solidWorld(a.pos.x, a.pos.y, kPlayerRadius * 0.9f)) {
        return;
    }
    const float dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int dist = 4; dist <= 40; dist += 4) {
        for (auto& d : dirs) {
            const float nx = a.pos.x + d[0] * static_cast<float>(dist);
            const float ny = a.pos.y + d[1] * static_cast<float>(dist);
            if (!map_.solidWorld(nx, ny, kPlayerRadius)) {
                a.pos.x = nx;
                a.pos.y = ny;
                a.vel = {};
                return;
            }
        }
    }
    const int sx = std::max(0, std::min(map_.width() - 1, static_cast<int>(a.pos.x / kTile)));
    const int sy = std::max(0, std::min(map_.height() - 1, static_cast<int>(a.pos.y / kTile)));
    for (int r = 1; r <= 18; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) {
                    continue;
                }
                const int tx = sx + dx;
                const int ty = sy + dy;
                if (!tileOpen(tx, ty)) {
                    continue;
                }
                const Vec2 c = tileCenter(tx, ty);
                if (!map_.solidWorld(c.x, c.y, kPlayerRadius)) {
                    a.pos = c;
                    a.vel = {};
                    a.botWayX = a.botWayY = -1;
                    return;
                }
            }
        }
    }
}

bool World::tileOpen(int tx, int ty) const {
    return map_.inBounds(tx, ty) && !map_.solid(tx, ty);
}

Vec2 World::tileCenter(int tx, int ty) const {
    return {tx * kTile + kTile * 0.5f, ty * kTile + kTile * 0.5f};
}

void World::setBotWay(Actor& a, int tx, int ty) {
    a.botPrevX = a.botWayX;
    a.botPrevY = a.botWayY;
    a.botWayX = tx;
    a.botWayY = ty;
    a.botTarget = tileCenter(tx, ty);
    Vec2 d = a.botTarget - a.pos;
    if (d.length2() > 0.0001f) {
        a.botSteer = d.normalized();
    }
}

Vec2 World::botSlide(const Actor& a, Vec2 want) const {
    if (want.length2() < 0.0001f) {
        return {};
    }
    const Vec2 n = want.normalized();
    const float look = 16.0f;
    const bool ahead = !map_.solidWorld(a.pos.x + n.x * look, a.pos.y + n.y * look, kPlayerRadius);
    const bool xOk = !map_.solidWorld(a.pos.x + n.x * look, a.pos.y, kPlayerRadius);
    const bool yOk = !map_.solidWorld(a.pos.x, a.pos.y + n.y * look, kPlayerRadius);
    if (ahead && xOk && yOk) {
        return n;
    }
    if (xOk && std::fabs(n.x) >= std::fabs(n.y) && std::fabs(n.x) > 0.08f) {
        return {n.x > 0.0f ? 1.0f : -1.0f, 0.0f};
    }
    if (yOk && std::fabs(n.y) > 0.08f) {
        return {0.0f, n.y > 0.0f ? 1.0f : -1.0f};
    }
    if (xOk && std::fabs(n.x) > 0.08f) {
        return {n.x > 0.0f ? 1.0f : -1.0f, 0.0f};
    }
    const Vec2 left{-n.y, n.x};
    const Vec2 right{n.y, -n.x};
    const Vec2 side = a.botWallDir >= 0 ? left : right;
    const Vec2 other = a.botWallDir >= 0 ? right : left;
    if (!map_.solidWorld(a.pos.x + side.x * look, a.pos.y + side.y * look, kPlayerRadius)) {
        return side;
    }
    if (!map_.solidWorld(a.pos.x + other.x * look, a.pos.y + other.y * look, kPlayerRadius)) {
        return other;
    }
    return n;
}

bool World::refreshBotPath(Actor& a, Vec2 goal) {
    int sx = static_cast<int>(a.pos.x / kTile);
    int sy = static_cast<int>(a.pos.y / kTile);
    int gx = static_cast<int>(goal.x / kTile);
    int gy = static_cast<int>(goal.y / kTile);
    sx = std::max(0, std::min(map_.width() - 1, sx));
    sy = std::max(0, std::min(map_.height() - 1, sy));
    gx = std::max(0, std::min(map_.width() - 1, gx));
    gy = std::max(0, std::min(map_.height() - 1, gy));

    const Vec2 delta = goal - a.pos;
    const float dist = delta.length();
    if (dist > 18.0f) {
        const Vec2 dir = delta.normalized();
        const Vec2 ahead = a.pos + dir * std::min(kTile * 3.0f, dist * 0.65f);
        if (!map_.lineBlocked(a.pos, ahead) && !map_.solidWorld(ahead.x, ahead.y, kPlayerRadius)) {
            setBotWay(a, static_cast<int>(ahead.x / kTile), static_cast<int>(ahead.y / kTile));
            return true;
        }
    }

    struct Node {
        int16_t x, y, parent;
    };
    Node nodes[192];
    int q[192];
    int n = 0, qh = 0, qt = 0;
    nodes[n] = {static_cast<int16_t>(sx), static_cast<int16_t>(sy), -1};
    q[qt++] = n++;

    int best = 0;
    int bestD = std::abs(gx - sx) + std::abs(gy - sy);
    bool reached = false;
    const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (qh < qt && n < 192) {
        const int i = q[qh++];
        const int x = nodes[i].x;
        const int y = nodes[i].y;
        const int d = std::abs(gx - x) + std::abs(gy - y);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
        if (x == gx && y == gy) {
            best = i;
            reached = true;
            break;
        }
        for (auto& o : nb) {
            const int nx = x + o[0];
            const int ny = y + o[1];
            if (!(nx == sx && ny == sy) && !tileOpen(nx, ny)) {
                continue;
            }
            bool seen = false;
            for (int k = 0; k < n; ++k) {
                if (nodes[k].x == nx && nodes[k].y == ny) {
                    seen = true;
                    break;
                }
            }
            if (seen) {
                continue;
            }
            nodes[n] = {static_cast<int16_t>(nx), static_cast<int16_t>(ny), static_cast<int16_t>(i)};
            q[qt++] = n;
            ++n;
            if (n >= 192) {
                break;
            }
        }
    }

    if (!reached && bestD <= 2) {
        a.botWayX = a.botWayY = -1;
        return false;
    }

    int chain[16];
    int sc = 0;
    for (int i = best; i >= 0 && sc < 16; i = nodes[i].parent) {
        chain[sc++] = i;
        if (nodes[i].parent < 0) {
            break;
        }
    }
    if (sc < 2) {
        a.botWayX = a.botWayY = -1;
        return false;
    }

    int idx = sc - 1 - std::min(4, sc - 1);
    if (idx < 0) {
        idx = 0;
    }
    auto atChain = [&](int i) {
        return nodes[chain[i]];
    };
    if (atChain(idx).x == a.botPrevX && atChain(idx).y == a.botPrevY && idx > 0) {
        --idx;
    }
    if (atChain(idx).x == sx && atChain(idx).y == sy) {
        if (idx > 0) {
            --idx;
        } else {
            a.botWayX = a.botWayY = -1;
            return false;
        }
    }
    if (atChain(idx).x == a.botPrevX && atChain(idx).y == a.botPrevY) {
        return false;
    }
    setBotWay(a, atChain(idx).x, atChain(idx).y);
    return true;
}

void World::recoverBot(Actor& a, Vec2 goal) {
    const int sx = std::max(0, std::min(map_.width() - 1, static_cast<int>(a.pos.x / kTile)));
    const int sy = std::max(0, std::min(map_.height() - 1, static_cast<int>(a.pos.y / kTile)));
    const int gx = static_cast<int>(goal.x / kTile);
    const int gy = static_cast<int>(goal.y / kTile);
    const int dx[4] = {1, 0, -1, 0};
    const int dy[4] = {0, 1, 0, -1};
    int best = -1;
    int bestScore = -1000;
    for (int dir = 0; dir < 4; ++dir) {
        const int nx = sx + dx[dir];
        const int ny = sy + dy[dir];
        if (!tileOpen(nx, ny)) {
            continue;
        }
        if (nx == a.botPrevX && ny == a.botPrevY) {
            continue;
        }
        int score = (std::abs(gx - sx) + std::abs(gy - sy)) - (std::abs(gx - nx) + std::abs(gy - ny));
        score += 2 * tileOpen(nx + dx[dir], ny + dy[dir]);
        if (dir == (a.botWallDir >= 0 ? 1 : 3) || dir == (a.botWallDir >= 0 ? 0 : 2)) {
            score += 1;
        }
        if (score > bestScore) {
            bestScore = score;
            best = dir;
        }
    }
    if (best >= 0) {
        setBotWay(a, sx + dx[best], sy + dy[best]);
        a.botLockT = 0.45f;
        return;
    }
    a.botWallDir = -a.botWallDir;
    wanderBot(a);
}

void World::wanderBot(Actor& a) {
    const int sx = std::max(0, std::min(map_.width() - 1, static_cast<int>(a.pos.x / kTile)));
    const int sy = std::max(0, std::min(map_.height() - 1, static_cast<int>(a.pos.y / kTile)));
    const int sideX = a.botWallDir >= 0 ? -1 : 1;
    for (int tries = 0; tries < 28; ++tries) {
        int dx = irand(15) - 7;
        int dy = irand(15) - 7;
        if (tries < 8) {
            dx = sideX * (2 + irand(6));
            dy = irand(9) - 4;
        }
        if (dx * dx + dy * dy < 4) {
            continue;
        }
        const int nx = sx + dx;
        const int ny = sy + dy;
        if (!tileOpen(nx, ny) || (nx == a.botPrevX && ny == a.botPrevY)) {
            continue;
        }
        if (map_.solidWorld(tileCenter(nx, ny).x, tileCenter(nx, ny).y, kPlayerRadius)) {
            continue;
        }
        setBotWay(a, nx, ny);
        a.botLockT = 1.1f;
        a.botFail = 0;
        return;
    }
    const float turn = a.angle + a.botWallDir * (kPi * 0.7f);
    a.botSteer = {std::cos(turn), std::sin(turn)};
    a.botWayX = a.botWayY = -1;
    a.botLockT = 0.6f;
}

Actor* World::nearestEnemy(const Actor& a) {
    Actor* best = nullptr;
    float bestD = 1e9f;
    for (auto& o : actors_) {
        if (!o.active || !o.alive || o.id == a.id || !hostile(a, o)) {
            continue;
        }
        const float d = (o.pos - a.pos).length2();
        if (d < bestD) {
            bestD = d;
            best = &o;
        }
    }
    return best;
}

void World::tryFire(Actor& a, Camera* cam) {
    if (!a.alive || a.cooldown > 0.0f || a.reloadT > 0.0f) {
        return;
    }
    const WeaponDef& w = weaponDef(a.weapon);
    if (w.melee) {
        a.cooldown = weaponCooldown(w) * (a.bot ? tune(difficulty_).shotDelay : 1.0f);
        a.muzzleT = 0.05f;
        a.radarT = kRadarPing;
        if (cam && a.id == localId_) {
            cam->shake(w.shake, 0.08f);
        }
        for (auto& o : actors_) {
            if (!o.active || !o.alive || o.id == a.id || !hostile(a, o)) {
                continue;
            }
            const Vec2 d = o.pos - a.pos;
            const float reach = a.zombie ? 30.0f : w.range;
            if (d.length() < reach) {
                const float facing = std::cos(a.angle) * d.x + std::sin(a.angle) * d.y;
                if (facing > 0.0f) {
                    o.hp -= static_cast<int>(w.damage);
                    if (o.hp <= 0) {
                        killActor(o);
                        a.kills++;
                    }
                }
            }
        }
        if (mode_ == GameMode::ProtectBase) {
            const Team foe = huntBaseTeam(a.team, a.pos);
            if (baseAlive(foe)) {
                const Vec2 d = basePos(foe) - a.pos;
                const float reach = a.zombie ? 30.0f : w.range;
                if (d.length() < reach) {
                    const float facing = std::cos(a.angle) * d.x + std::sin(a.angle) * d.y;
                    if (facing > 0.0f) {
                        hurtBase(foe, w.damage, a.id);
                    }
                }
            }
        }
        return;
    }
    if (a.mag <= 0) {
        if (a.reserve > 0) {
            a.reloadT = w.reload;
        }
        return;
    }
    a.mag -= 1;
    a.cooldown = weaponCooldown(w) * (a.bot ? tune(difficulty_).shotDelay : 1.0f);
    a.muzzleT = w.kind == GunKind::Grenade ? 0.18f : 0.07f;
    a.radarT = kRadarPing;
    if (cam && a.id == localId_ && w.kind != GunKind::Grenade) {
        cam->shake(w.shake, 0.12f + w.shake * 0.01f);
    }
    if (w.kind != GunKind::Grenade) {
        spawnParticle(particles_, a.pos + Vec2{std::cos(a.angle), std::sin(a.angle)} * 16.0f,
                      Vec2{std::cos(a.angle), std::sin(a.angle)} * 40.0f, {255, 220, 80, 255}, 0.12f, 4.0f);
    }

    const Vec2 hold = gunHoldWorld(a);
    const float spreadMul = a.bot ? tune(difficulty_).spreadMul : 1.0f;
    for (int p = 0; p < w.pellets; ++p) {
        const float spread = ((irand(200) / 100.0f) - 1.0f) * w.spread * spreadMul;
        const float ang = a.angle + spread + ((irand(200) / 100.0f) - 1.0f) * w.recoil * spreadMul;
        Bullet* b = nullptr;
        for (auto& slot : bullets_) {
            if (!slot.active) {
                b = &slot;
                break;
            }
        }
        if (!b) {
            bullets_.push_back({});
            b = &bullets_.back();
        }
        const Vec2 dir{std::cos(ang), std::sin(ang)};
        b->active = true;
        b->origin = hold;
        b->pos = hold + dir * 16.0f;
        b->owner = a.id;
        b->team = a.team;
        b->nade = w.kind == GunKind::Grenade;
        b->weapon = a.weapon;
        if (b->nade) {
            b->vel = dir * 210.0f;
            b->life = 1.5f;
            b->damage = w.damage;
        } else {
            b->pos = hold + dir * 16.0f;
            b->vel = dir * w.bulletSpeed;
            b->life = w.range / w.bulletSpeed;
            b->damage = w.damage * (a.bot ? tune(difficulty_).botDmgMul : 1.0f);
        }
    }
}

Vec2 World::pickPatrolPoint(const Actor& a) const {
    const int w = map_.width();
    const int h = map_.height();
    if (w < 6 || h < 6) {
        return a.pos;
    }
    const int quad = (a.id * 3 + a.botJob + static_cast<int>(tick_)) % 4;
    int x0 = 2;
    int y0 = 2;
    int x1 = std::max(3, w / 2);
    int y1 = std::max(3, h / 2);
    if (quad == 1) {
        x0 = w / 2;
        x1 = w - 3;
    } else if (quad == 2) {
        y0 = h / 2;
        y1 = h - 3;
    } else if (quad == 3) {
        x0 = w / 2;
        y0 = h / 2;
        x1 = w - 3;
        y1 = h - 3;
    }
    if (x1 <= x0) {
        x1 = x0 + 1;
    }
    if (y1 <= y0) {
        y1 = y0 + 1;
    }
    for (int t = 0; t < 22; ++t) {
        const int tx = x0 + irand(x1 - x0);
        const int ty = y0 + irand(y1 - y0);
        if (!tileOpen(tx, ty)) {
            continue;
        }
        const Vec2 c = tileCenter(tx, ty);
        if (!map_.solidWorld(c.x, c.y, kPlayerRadius)) {
            return c;
        }
    }
    return a.pos;
}

Vec2 World::basePos(Team team) const {
    return bases_[teamIndex(team)].pos;
}

bool World::baseAlive(Team team) const {
    const Base& b = bases_[teamIndex(team)];
    return b.active && b.hp > 0;
}

Team World::huntBaseTeam(Team from, Vec2 pos) const {
    Team best = from;
    float bestD = 1.0e12f;
    for (int i = 0; i < teamCount_; ++i) {
        if (!bases_[i].active || bases_[i].hp <= 0 || bases_[i].team == from) {
            continue;
        }
        const float d = (bases_[i].pos - pos).length2();
        if (d < bestD) {
            bestD = d;
            best = bases_[i].team;
        }
    }
    if (best == from) {
        return teamFromIndex((teamIndex(from) + 1) % std::max(2, teamCount_));
    }
    return best;
}

Vec2 World::pickDefendPoint(Team team) const {
    const Vec2 c = basePos(team);
    for (int t = 0; t < 18; ++t) {
        const float ang = (static_cast<float>(irand(628)) / 100.0f);
        const float rad = 36.0f + static_cast<float>(irand(70));
        const Vec2 p{c.x + std::cos(ang) * rad, c.y + std::sin(ang) * rad};
        if (!map_.solidWorld(p.x, p.y, kPlayerRadius)) {
            return p;
        }
    }
    return c;
}

Vec2 World::spawnAtBase(Team team, int slot) const {
    const Vec2 c = basePos(team);
    const float ring = 28.0f + static_cast<float>(slot % 6) * 10.0f;
    const float ang = static_cast<float>(slot) * 0.95f;
    const Vec2 first{c.x + std::cos(ang) * ring, c.y + std::sin(ang) * ring};
    if (!map_.solidWorld(first.x, first.y, kPlayerRadius)) {
        return first;
    }
    for (int t = 0; t < 24; ++t) {
        const float a = ang + static_cast<float>(t) * 0.45f;
        const float r = 24.0f + static_cast<float>(t) * 6.0f;
        const Vec2 p{c.x + std::cos(a) * r, c.y + std::sin(a) * r};
        if (!map_.solidWorld(p.x, p.y, kPlayerRadius)) {
            return p;
        }
    }
    return map_.spawn(team, slot, teamCount_);
}

void World::setupBases() {
    for (auto& b : bases_) {
        b = Base{};
    }
    if (mode_ != GameMode::ProtectBase) {
        return;
    }
    for (int i = 0; i < teamCount_; ++i) {
        Base& b = bases_[i];
        b.team = teamFromIndex(i);
        b.pos = map_.teamHome(i, teamCount_);
        b.active = true;
        b.maxHp = kBaseMaxHp;
        b.hp = kBaseMaxHp;
    }
    for (int i = 0; i < teamCount_; ++i) {
        for (int j = i + 1; j < teamCount_; ++j) {
            if ((bases_[i].pos - bases_[j].pos).length2() < 64.0f * 64.0f) {
                bases_[j].pos = map_.teamHome(j, teamCount_);
            }
        }
    }
}

void World::hurtBase(Team victim, float dmg, int owner) {
    if (mode_ != GameMode::ProtectBase) {
        return;
    }
    Base& b = bases_[teamIndex(victim)];
    if (!b.active) {
        return;
    }
    if (b.hp > 0 && dmg > 0.0f) {
        b.hp -= static_cast<int>(dmg);
        b.hurtT = 0.45f;
        emitBurst(b.pos, 8, 70.0f, {255, 80, 30, 255}, 0.28f, 4.0f);
    }
    if (b.hp > 0 || matchOver_) {
        if (b.hp < 0) {
            b.hp = 0;
        }
        return;
    }
    b.hp = 0;
    b.active = false;
    const char* letter = teamLetter(b.team);
    if (owner >= 0 && owner < kMaxPlayers && actors_[owner].active) {
        std::snprintf(banner_, sizeof(banner_), "BASE %s DESTROYED BY TEAM %s", letter,
                      teamLetter(actors_[owner].team));
    } else {
        std::snprintf(banner_, sizeof(banner_), "BASE %s DESTROYED", letter);
    }
    bannerT_ = 4.5f;
}

bool World::hitBases(Vec2 pos, float dmg, int owner, Team ownerTeam) {
    if (mode_ != GameMode::ProtectBase) {
        return false;
    }
    for (auto& b : bases_) {
        if (!b.active || b.hp <= 0 || b.team == ownerTeam) {
            continue;
        }
        if ((b.pos - pos).length2() <= kBaseHitR * kBaseHitR) {
            hurtBase(b.team, dmg, owner);
            return true;
        }
    }
    return false;
}

void World::scanNukes() {
    nukes_.clear();
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            if (map_.at(x, y) == Tile::Nuclear) {
                NukeBlock n;
                n.active = true;
                n.tx = x;
                n.ty = y;
                n.hp = kNukeHits;
                nukes_.push_back(n);
            }
        }
    }
}

void World::hitNuclear(int tx, int ty, int owner, Camera* cam) {
    for (auto& n : nukes_) {
        if (!n.active || n.tx != tx || n.ty != ty) {
            continue;
        }
        n.hp -= 1;
        const Vec2 pos{tx * kTile + kTile * 0.5f, ty * kTile + kTile * 0.5f};
        emitBurst(pos, 6, 50.0f, {255, 220, 40, 255}, 0.18f, 4.0f);
        if (n.hp <= 0) {
            explodeNuclear(n, owner, cam);
        }
        return;
    }
}

void World::explodeNuclear(NukeBlock& n, int owner, Camera* cam) {
    n.active = false;
    n.hp = 0;
    map_.set(n.tx, n.ty, Tile::Floor);
    freeMinimap();
    const Vec2 pos{n.tx * kTile + kTile * 0.5f, n.ty * kTile + kTile * 0.5f};
    emitNadeParticles(pos, 1);
    Scorch* sc = nullptr;
    for (auto& s : scorches_) {
        if (!s.active) {
            sc = &s;
            break;
        }
    }
    if (!sc && !scorches_.empty()) {
        sc = &scorches_[0];
    }
    if (sc) {
        sc->active = true;
        sc->pos = pos;
        sc->life = 8.0f;
    }
    if (cam) {
        const Vec2 c = cam->pos + Vec2{cam->viewW() * 0.5f, cam->viewH() * 0.5f};
        float dist2 = (c - pos).length2();
        if (dist2 < 1000.0f) {
            dist2 = 1000.0f;
        }
        cam->shake(80000.0f / dist2, 0.45f);
    }
    constexpr float dmgBase = 90.0f;
    for (auto& a : actors_) {
        if (!a.active || !a.alive) {
            continue;
        }
        if (map_.lineBlocked(pos, a.pos)) {
            continue;
        }
        float distance = (pos - a.pos).length();
        if (distance < 40.0f) {
            distance = 40.0f;
        } else if (distance > 180.0f) {
            continue;
        }
        float dmg = (40.0f / distance) * dmgBase;
        if (a.id == localId_) {
            dmg *= tune(difficulty_).incomingMul;
        }
        a.hp -= static_cast<int>(dmg);
        spawnParticle(particles_, a.pos, {}, {180, 30, 30, 255}, 0.28f, 4.0f);
        if (a.hp <= 0) {
            const int victim = a.id;
            killActor(a);
            if (owner >= 0 && owner != victim && owner < kMaxPlayers && actors_[owner].active) {
                actors_[owner].kills++;
            }
        }
    }
}

void World::packSnapshot(NetSnapshot& snap) const {
    snap.tick = tick_;
    snap.wave = static_cast<uint8_t>(std::max(0, wave_));
    snap.extra = static_cast<uint8_t>((matchOver_ ? 1 : 0) | ((winnerId_ + 1) << 1));
    auto packHp = [](int hp, int maxHp) -> uint8_t {
        if (maxHp <= 0 || hp <= 0) {
            return 0;
        }
        const int v = (hp * 255) / maxHp;
        return static_cast<uint8_t>(v < 1 ? 1 : (v > 255 ? 255 : v));
    };
    if (mode_ == GameMode::ProtectBase) {
        snap.teamCount = static_cast<uint8_t>(teamCount_);
        for (int i = 0; i < kMaxTeams; ++i) {
            snap.baseHp[i] = packHp(bases_[i].hp, bases_[i].maxHp);
        }
    } else {
        snap.teamCount = static_cast<uint8_t>(teamCount_);
        for (int i = 0; i < kMaxTeams; ++i) {
            snap.baseHp[i] = 255;
        }
    }
    snap.count = 0;
    for (const auto& a : actors_) {
        if (!a.active) {
            continue;
        }
        auto& s = snap.actors[snap.count++];
        s.id = static_cast<uint8_t>(a.id);
        s.team = static_cast<uint8_t>(a.team);
        s.hp = static_cast<uint8_t>(std::max(0, a.hp));
        s.weapon = static_cast<uint8_t>(a.weapon);
        s.x = static_cast<uint16_t>(a.pos.x * 10.0f);
        s.y = static_cast<uint16_t>(a.pos.y * 10.0f);
        s.angle = static_cast<int16_t>(deg(a.angle) * 10.0f);
        s.flags = static_cast<uint8_t>((a.alive ? 1 : 0) | (a.zombie ? 2 : 0) | ((a.skinId & 7) << 2));
    }
}

void World::assignBotJobs() {
    Actor* me = local();
    int allies[kMaxPlayers];
    int enemies[kMaxPlayers];
    int nAlly = 0;
    int nEnemy = 0;
    for (auto& a : actors_) {
        if (!a.active || !a.bot) {
            continue;
        }
        a.botBuddy = -1;
        if (a.zombie) {
            a.botJob = (a.id % 2 == 0) ? kJobHunt : kJobPatrol;
            a.botHome = pickPatrolPoint(a);
            a.botJobT = 5.0f + static_cast<float>(a.id % 6);
            continue;
        }
        if (mode_ != GameMode::LastSurvivor && me && me->alive && !hostile(a, *me)) {
            allies[nAlly++] = a.id;
        } else {
            enemies[nEnemy++] = a.id;
        }
    }

    const int followN = nAlly / 2;
    for (int i = 0; i < nAlly; ++i) {
        Actor& a = actors_[allies[i]];
        a.botJob = (i < followN) ? kJobFollow : kJobPatrol;
        a.botHome = (mode_ == GameMode::ProtectBase && a.botJob == kJobPatrol) ? pickDefendPoint(a.team)
                                                                              : pickPatrolPoint(a);
        a.botJobT = 6.0f + static_cast<float>(i % 8);
    }

    int squadIds[kMaxPlayers];
    int nSquad = 0;
    for (int i = 0; i < nEnemy; ++i) {
        Actor& a = actors_[enemies[i]];
        if (mode_ == GameMode::LastSurvivor) {
            a.botJob = (i % 2 == 0) ? kJobHunt : kJobPatrol;
        } else if (mode_ == GameMode::ProtectBase) {
            a.botJob = (i % 3 == 2) ? kJobPatrol : kJobHunt;
        } else {
            const int r = i % 3;
            a.botJob = (r == 0) ? kJobHunt : (r == 1 ? kJobSquad : kJobPatrol);
        }
        if (mode_ == GameMode::ProtectBase) {
            const Team foe = huntBaseTeam(a.team, a.pos);
            a.botHome = (a.botJob == kJobHunt) ? basePos(foe) : pickDefendPoint(a.team);
        } else {
            a.botHome = pickPatrolPoint(a);
        }
        a.botJobT = 5.0f + static_cast<float>(i % 9);
        if (a.botJob == kJobSquad) {
            squadIds[nSquad++] = a.id;
        }
    }
    for (int i = 0; i + 1 < nSquad; i += 2) {
        actors_[squadIds[i]].botBuddy = squadIds[i + 1];
        actors_[squadIds[i + 1]].botBuddy = squadIds[i];
    }
    if (nSquad % 2 == 1) {
        Actor& last = actors_[squadIds[nSquad - 1]];
        last.botJob = kJobPatrol;
        last.botBuddy = -1;
    }
}

void World::updateBots(float dt) {
    const DiffTune t = tune(difficulty_);
    Actor* me = local();
    const int mapW = map_.width();
    int pathsLeft = mapW >= 400 ? 3 : (mapW >= 200 ? 4 : 8);
    const int aiStride = mapW >= 400 ? 3 : (mapW >= 200 ? 2 : 1);
    int i = 0;
    for (auto& a : actors_) {
        if (!a.active || !a.bot || !a.alive) {
            continue;
        }
        ++i;
        const bool nearLocal = me && me->alive && (a.pos - me->pos).length2() < 240.0f * 240.0f;
        const bool fullAi = nearLocal || aiStride <= 1 || ((a.id + static_cast<int>(tick_)) % aiStride) == 0;
        if (!fullAi) {
            if (a.botFocus >= 0 && a.botFocus < kMaxPlayers) {
                const Actor& focus = actors_[a.botFocus];
                if (focus.active && focus.alive && hostile(a, focus)) {
                    a.botFocusPos = focus.pos;
                    const float maxRange = a.zombie ? 28.0f : weaponDef(a.weapon).range * t.rangeMul;
                    const float d2 = (focus.pos - a.pos).length2();
                    a.botFire = d2 < maxRange * maxRange;
                } else {
                    a.botFocus = -1;
                    a.botFire = false;
                }
            }
            Vec2 steer = a.botSteer;
            if (a.botHold || a.botFire) {
                steer = {};
            } else if (a.botWayX >= 0) {
                steer = a.botTarget - a.pos;
            }
            if (steer.length2() > 0.01f) {
                steer = botSlide(a, steer);
            }
            const Vec2 prev = a.pos;
            applyAnalog(a, steer.x, steer.y, dt, false);
            moveActor(a, dt);
            const Vec2 delta = a.pos - prev;
            Vec2 face = delta;
            if (a.botFire || a.botFocus >= 0) {
                face = a.botFocusPos - a.pos;
            }
            if (face.length2() > 0.0001f) {
                a.angle = lerpAngle(a.angle, angleOf(face), clamp(dt * (a.botFire ? 9.0f : 6.0f), 0.0f, 1.0f));
            }
            if (a.botFire) {
                a.botThink += dt;
                if (a.botThink >= t.reaction && (a.mag > 0 || a.zombie)) {
                    if (a.zombie || (irand(100) / 100.0f) <= t.fireChance) {
                        tryFire(a, nullptr);
                    }
                }
            }
            continue;
        }
        unstickActor(a);
        if (a.flashT > 0.0f && (a.botWayX < 0 || (a.pos - a.botTarget).length() < 18.0f)) {
            wanderBot(a);
        }
        Actor* enemy = a.flashT > 0.0f ? nullptr : nearestEnemy(a);
        Vec2 toEnemy{};
        bool los = false;
        float enemyDist = 0.0f;
        if (enemy) {
            toEnemy = enemy->pos - a.pos;
            enemyDist = toEnemy.length();
            if (enemyDist > 1.0f) {
                toEnemy = toEnemy * (1.0f / enemyDist);
            } else {
                toEnemy = {std::cos(a.angle), std::sin(a.angle)};
            }
        }

        const float maxRange = a.zombie ? 28.0f : weaponDef(a.weapon).range * t.rangeMul;
        const bool inWall = map_.solidWorld(a.pos.x, a.pos.y, kPlayerRadius * 0.9f);
        if (enemy && enemyDist < maxRange * 1.05f) {
            los = !sightBlocked(a.pos, enemy->pos, maxRange * 1.05f);
        }
        const bool inRange = enemy && los && enemyDist < maxRange;

        if (a.botBuddy >= 0) {
            const Actor& buddy = actors_[a.botBuddy];
            if (!buddy.active || !buddy.alive || hostile(a, buddy)) {
                a.botBuddy = -1;
                if (a.botJob == kJobSquad) {
                    a.botJob = kJobPatrol;
                    a.botHome = pickPatrolPoint(a);
                    a.botJobT = 6.0f;
                }
            }
        }

        a.botJobT -= dt;
        if (a.botJob == kJobPatrol &&
            (a.botJobT <= 0.0f || (a.pos - a.botHome).length() < 36.0f)) {
            a.botHome = mode_ == GameMode::ProtectBase ? pickDefendPoint(a.team) : pickPatrolPoint(a);
            a.botJobT = 6.0f + static_cast<float>(irand(8));
            a.botWayX = a.botWayY = -1;
        }

        const bool ally = me && me->alive && !a.zombie && !hostile(a, *me);
        const float followDist = (me && me->alive) ? (me->pos - a.pos).length() : 0.0f;
        const float aggro = a.zombie ? 170.0f : 150.0f;
        const bool press = inRange || (enemy && enemyDist < aggro);

        Vec2 goal = a.botHome.length2() > 1.0f ? a.botHome : (a.pos + Vec2{std::cos(a.angle), std::sin(a.angle)} * 80.0f);
        bool holdFollow = false;

        auto flankGoal = [&](const Actor& prey) {
            const Vec2 to = prey.pos - a.pos;
            const float d = to.length();
            if (d < 130.0f) {
                return prey.pos;
            }
            const Vec2 n = d > 1.0f ? to * (1.0f / d) : Vec2{1, 0};
            const Vec2 side{-n.y, n.x};
            const float sign = (a.id % 2 == 0) ? 1.0f : -1.0f;
            const float wide = 48.0f + static_cast<float>((a.id % 3) * 28);
            return prey.pos + side * (sign * wide);
        };

        if (press && a.botJob != kJobFollow && a.botJob != kJobSquad &&
            (mode_ != GameMode::ProtectBase || enemyDist < 80.0f)) {
            goal = enemy->pos;
        } else if (a.botJob == kJobFollow && ally && me) {
            const float space = 72.0f + static_cast<float>((a.id % 3) * 26);
            if (press && enemyDist < 180.0f) {
                goal = enemy->pos;
            } else if (followDist < space) {
                holdFollow = true;
                goal = a.pos;
            } else {
                goal = me->pos;
            }
        } else if (a.botJob == kJobSquad && a.botBuddy >= 0) {
            const Actor& buddy = actors_[a.botBuddy];
            const float bd = (buddy.pos - a.pos).length();
            if (bd > 110.0f) {
                goal = buddy.pos;
            } else if (press) {
                goal = flankGoal(*enemy);
            } else {
                goal = buddy.pos;
                if (bd < 42.0f) {
                    holdFollow = true;
                    goal = a.pos;
                }
            }
        } else if (a.botJob == kJobHunt) {
            Actor* prey = enemy;
            if (me && me->alive && hostile(a, *me)) {
                prey = me;
            }
            if (mode_ == GameMode::ProtectBase) {
                const Team foe = huntBaseTeam(a.team, a.pos);
                goal = baseAlive(foe) ? basePos(foe) : a.botHome;
                if (press && enemy && enemyDist < 80.0f) {
                    goal = flankGoal(*enemy);
                }
            } else if (prey) {
                goal = flankGoal(*prey);
            } else {
                goal = a.botHome;
            }
        } else {
            if (press) {
                goal = enemy->pos;
            } else {
                goal = a.botHome;
            }
        }

        const Team foeTeam = mode_ == GameMode::ProtectBase ? huntBaseTeam(a.team, a.pos) : a.team;
        Vec2 enemyBase = mode_ == GameMode::ProtectBase ? basePos(foeTeam) : a.pos;
        float baseDist = 0.0f;
        bool canHitBase = false;
        if (mode_ == GameMode::ProtectBase && !a.zombie && baseAlive(foeTeam)) {
            baseDist = (enemyBase - a.pos).length();
            if (baseDist < maxRange * 1.05f) {
                canHitBase = !sightBlocked(a.pos, enemyBase, maxRange * 1.05f);
            }
            if (a.botJob == kJobHunt && !(press && enemyDist < 80.0f)) {
                goal = enemyBase;
            }
        }

        auto faceToward = [&](Vec2 dir, float rate) {
            if (dir.length2() < 0.0001f) {
                return;
            }
            a.angle = lerpAngle(a.angle, angleOf(dir), clamp(dt * rate, 0.0f, 1.0f));
        };

        const bool far = !enemy || enemyDist > std::max(220.0f, maxRange * 1.15f);
        const bool cheap = far && !inWall && !holdFollow && !canHitBase && a.botWayX >= 0 && ((i + tick_) & 1) &&
                           (a.pos - a.botTarget).length2() > 400.0f;

        a.botPathT += dt;
        const bool reachedWay = a.botWayX >= 0 && (a.pos - a.botTarget).length() < 14.0f;
        const bool needPath =
            !inRange && !canHitBase && !holdFollow && (inWall || a.botWayX < 0 || reachedWay || a.botPathT > 1.15f);
        if (needPath && pathsLeft > 0 && !cheap) {
            a.botPathT = 0.0f;
            --pathsLeft;
            if (reachedWay) {
                a.botPrevX = a.botWayX;
                a.botPrevY = a.botWayY;
                a.botWayX = a.botWayY = -1;
            }
            if (!refreshBotPath(a, goal)) {
                if (a.botFail >= 1) {
                    wanderBot(a);
                } else {
                    recoverBot(a, goal);
                }
            }
        }

        Vec2 steer;
        a.botLockT -= dt;
        if (holdFollow) {
            steer = {};
        } else if (inRange) {
            if (a.zombie && enemyDist > 20.0f) {
                steer = toEnemy;
            } else if (!a.zombie && enemyDist > maxRange * 0.78f) {
                steer = toEnemy * 0.4f;
            } else {
                steer = {};
            }
        } else if (canHitBase) {
            if (baseDist > maxRange * 0.72f) {
                steer = enemyBase - a.pos;
            } else {
                steer = {};
            }
        } else if (a.botLockT > 0.0f && a.botSteer.length2() > 0.01f) {
            steer = a.botSteer;
        } else if (a.botWayX >= 0) {
            steer = a.botTarget - a.pos;
            if (steer.length2() < 4.0f) {
                a.botPrevX = a.botWayX;
                a.botPrevY = a.botWayY;
                a.botWayX = a.botWayY = -1;
                steer = {};
            }
        } else {
            steer = goal - a.pos;
        }
        if (!inRange && !canHitBase && !holdFollow) {
            steer = botSlide(a, steer);
        }

        const Vec2 prev = a.pos;
        applyAnalog(a, steer.x, steer.y, dt, false);
        moveActor(a, dt);

        const Vec2 delta = a.pos - prev;
        const float moved = delta.length();
        a.botStuckT += dt;
        if (a.botStuckT >= 0.28f) {
            const float traveled = (a.pos - a.botLast).length();
            const float goalDist = (goal - a.pos).length();
            const bool circling = traveled > 20.0f && goalDist > 48.0f && goalDist > a.botGoalD - 8.0f;
            const bool stuck = traveled < 10.0f && !inRange && !canHitBase && !holdFollow;
            a.botLast = a.pos;
            a.botGoalD = goalDist;
            a.botStuckT = 0.0f;
            if (stuck || circling) {
                ++a.botFail;
                a.botPrevX = a.botWayX;
                a.botPrevY = a.botWayY;
                a.botWayX = a.botWayY = -1;
                unstickActor(a);
                if (a.botFail >= 2) {
                    a.botWallDir = -a.botWallDir;
                    wanderBot(a);
                    a.botFail = 0;
                } else {
                    recoverBot(a, goal);
                    if (pathsLeft > 0) {
                        --pathsLeft;
                        refreshBotPath(a, goal);
                    }
                }
            } else if (traveled > 18.0f) {
                a.botFail = 0;
            }
        }

        if (a.zombie) {
            const Vec2 face = (enemy && enemyDist < 80.0f) ? toEnemy : (moved > 1.0f ? delta : (goal - a.pos));
            faceToward(face, 12.0f);
        } else if (inRange) {
            faceToward(toEnemy, 9.0f);
        } else if (canHitBase) {
            faceToward(enemyBase - a.pos, 9.0f);
        } else if (holdFollow && enemy) {
            faceToward(toEnemy, 7.0f);
        } else if (moved > 1.2f) {
            faceToward(delta, 6.0f);
        }

        a.botSteer = steer;
        a.botHold = holdFollow;
        a.botFire = inRange || canHitBase;
        if (inRange && enemy) {
            a.botFocus = enemy->id;
            a.botFocusPos = enemy->pos;
        } else if (canHitBase) {
            a.botFocus = -2;
            a.botFocusPos = enemyBase;
        } else if (enemy) {
            a.botFocus = enemy->id;
            a.botFocusPos = enemy->pos;
        } else {
            a.botFocus = -1;
        }

        bool wantShot = false;
        if (inRange || canHitBase) {
            a.botThink += dt;
            wantShot = a.botThink >= t.reaction;
        } else {
            a.botThink = 0.0f;
        }
        if (wantShot && (a.mag > 0 || a.zombie)) {
            if (a.zombie || (irand(100) / 100.0f) <= t.fireChance) {
                tryFire(a, nullptr);
            }
        } else if (a.mag <= 0 && a.reloadT <= 0.0f && !a.zombie) {
            a.reloadT = weaponDef(a.weapon).reload;
        }
    }
}

void World::emitBurst(Vec2 pos, int n, float speed, SDL_Color c, float life, float size) {
    for (int i = 0; i < n; ++i) {
        const float ang = (kPi * 2.0f * i) / static_cast<float>(std::max(1, n)) + (irand(40) / 100.0f);
        const float spd = speed * (0.45f + irand(70) / 100.0f);
        spawnParticle(particles_, pos, {std::cos(ang) * spd, std::sin(ang) * spd}, c, life, size);
    }
}

void World::emitNadeParticles(Vec2 pos, int kind) {
    // Counts / colors / sizes from original explosion.psi, flash.psi, smoke.psi.
    if (kind == 0) {
        const int n = 18;
        for (int i = 0; i < n; ++i) {
            const float ang = frand(0.0f, kPi * 2.0f);
            const float spd = frand(0.64f, 1.90f) * 100.0f;
            const float life = frand(0.06f, 0.20f);
            spawnFxParticle(particles_, pos + Vec2{frand(-2.0f, 2.0f), frand(-2.0f, 2.0f)},
                            {std::cos(ang) * spd, std::sin(ang) * spd}, 1, life, 0.92f, 3.12f,
                            {255, 255, 255, 250}, {255, 255, 255, 0}, -45.0f);
        }
    } else if (kind == 1) {
        const int n = 64;
        for (int i = 0; i < n; ++i) {
            const float ang = frand(0.0f, kPi * 2.0f);
            const float spd = frand(3.97f, 7.46f) * 100.0f;
            const float life = frand(0.16f, 0.44f);
            spawnFxParticle(particles_, pos + Vec2{frand(-2.0f, 2.0f), frand(-2.0f, 2.0f)},
                            {std::cos(ang) * spd, std::sin(ang) * spd}, 0, life, 3.12f, 0.52f,
                            {255, 20, 0, 255}, {255, 142, 0, 42}, frand(-12.0f, 12.0f));
        }
    }
}

void World::receiveFlash(Actor& a, float intensity) {
    if (!a.alive) {
        return;
    }
    a.flashT = 15.0f;
    a.flashIntensity = std::max(0.01f, intensity);
}

bool World::throughSmoke(Vec2 a, Vec2 b) const {
    const Vec2 d = b - a;
    const float len = d.length();
    if (len < 1.0f) {
        for (const auto& s : smokes_) {
            if (s.active && (a - s.pos).length() < s.radius * 0.85f) {
                return true;
            }
        }
        return false;
    }
    const Vec2 n = d * (1.0f / len);
    for (const auto& s : smokes_) {
        if (!s.active || s.radius < 8.0f) {
            continue;
        }
        const Vec2 to = s.pos - a;
        float t = to.x * n.x + to.y * n.y;
        t = clamp(t, 0.0f, len);
        const Vec2 closest = a + n * t;
        if ((closest - s.pos).length() < s.radius * 0.82f) {
            return true;
        }
    }
    return false;
}

bool World::sightBlocked(Vec2 a, Vec2 b, float maxDist) const {
    return map_.lineBlocked(a, b, maxDist) || throughSmoke(a, b);
}

void World::explodeGrenade(Bullet& nade, Camera* cam) {
    nade.active = false;
    const int kind = static_cast<int>(nade.weapon) - static_cast<int>(WeaponId::Flashbang);
    BlastFx* fx = nullptr;
    for (auto& b : blasts_) {
        if (!b.active) {
            fx = &b;
            break;
        }
    }
    if (!fx && !blasts_.empty()) {
        fx = &blasts_[0];
    }
    if (fx) {
        fx->active = true;
        fx->pos = nade.pos;
        fx->t = 0.0f;
        fx->life = kind == 2 ? 0.35f : (kind == 0 ? 0.35f : 0.55f);
        fx->kind = kind < 0 ? 1 : kind;
    }

    if (nade.weapon == WeaponId::HEGrenade || nade.weapon == WeaponId::Flashbang) {
        Scorch* sc = nullptr;
        for (auto& s : scorches_) {
            if (!s.active) {
                sc = &s;
                break;
            }
        }
        if (!sc && !scorches_.empty()) {
            sc = &scorches_[static_cast<size_t>(tick_ % scorches_.size())];
        }
        if (sc) {
            sc->active = true;
            sc->pos = nade.pos;
            sc->life = 8.0f;
        }
    }

    if (nade.weapon == WeaponId::Flashbang) {
        emitNadeParticles(nade.pos, 0);
        for (auto& a : actors_) {
            if (!a.active || !a.alive) {
                continue;
            }
            if (map_.lineBlocked(nade.pos, a.pos)) {
                continue;
            }
            if (nade.owner >= 0 && nade.owner < kMaxPlayers && actors_[nade.owner].active) {
                if (!hostile(actors_[nade.owner], a)) {
                    continue;
                }
            } else if (a.id == nade.owner) {
                continue;
            }
            if (a.id == localId_) {
                if (cam && !cam->onScreen(nade.pos.x - 12.0f, nade.pos.y - 12.0f, 24.0f, 24.0f)) {
                    continue;
                }
            } else {
                const Vec2 camPos = a.pos + Vec2{std::cos(a.angle), std::sin(a.angle)} * 30.0f;
                if (std::fabs(nade.pos.x - camPos.x) > kScreenW * 0.55f + 20.0f ||
                    std::fabs(nade.pos.y - camPos.y) > kScreenH * 0.55f + 20.0f) {
                    continue;
                }
            }
            float distance = (nade.pos - a.pos).length();
            if (distance < 20.0f) {
                distance = 20.0f;
            }
            Vec2 facing{std::cos(a.angle), std::sin(a.angle)};
            Vec2 gdir = nade.pos - a.pos;
            if (gdir.length2() < 0.0001f) {
                gdir = facing;
            } else {
                gdir = gdir.normalized();
            }
            float dot = facing.x * gdir.x + facing.y * gdir.y;
            dot = (dot + 1.0f) / 4.0f + 0.5f;
            receiveFlash(a, (20.0f / distance) * dot);
        }
    } else if (nade.weapon == WeaponId::HEGrenade) {
        emitNadeParticles(nade.pos, 1);
        if (cam) {
            const Vec2 c = cam->pos + Vec2{cam->viewW() * 0.5f, cam->viewH() * 0.5f};
            float dist2 = (c - nade.pos).length2();
            if (dist2 < 1000.0f) {
                dist2 = 1000.0f;
            }
            cam->shake(100000.0f / dist2, 0.50f);
        }
        const float dmgBase = nade.damage > 1.0f ? nade.damage : 100.0f;
        for (auto& a : actors_) {
            if (!a.active || !a.alive) {
                continue;
            }
            if (mode_ != GameMode::LastSurvivor && a.id != nade.owner && a.team == nade.team) {
                continue;
            }
            if (map_.lineBlocked(nade.pos, a.pos)) {
                continue;
            }
            float distance = (nade.pos - a.pos).length();
            if (distance < 40.0f) {
                distance = 40.0f;
            } else if (distance > 200.0f) {
                continue;
            }
            float dmg = (40.0f / distance) * dmgBase;
            if (a.id == localId_) {
                dmg *= tune(difficulty_).incomingMul;
            }
            a.hp -= static_cast<int>(dmg);
            spawnParticle(particles_, a.pos, {}, {180, 30, 30, 255}, 0.28f, 4.0f);
            emitBurst(a.pos, 5, 50.0f, {160, 20, 20, 255}, 0.25f, 3.0f);
            if (a.hp <= 0) {
                const int victim = a.id;
                killActor(a);
                if (nade.owner >= 0 && nade.owner != victim && nade.owner < kMaxPlayers &&
                    actors_[nade.owner].active) {
                    actors_[nade.owner].kills++;
                }
            }
        }
        if (mode_ == GameMode::ProtectBase) {
            for (auto& b : bases_) {
                if (!b.active || b.hp <= 0) {
                    continue;
                }
                float distance = (nade.pos - b.pos).length();
                if (distance > 200.0f) {
                    continue;
                }
                const bool pointBlank = distance < kTile * 1.35f;
                if (!pointBlank && map_.lineBlocked(nade.pos, b.pos)) {
                    continue;
                }
                if (distance < 40.0f) {
                    distance = 40.0f;
                }
                const float dmg = (40.0f / distance) * dmgBase * 8.0f;
                hurtBase(b.team, dmg, nade.owner);
                if (cam && nade.owner == localId_) {
                    cam->shake(5.5f, 0.28f);
                }
            }
        }
    } else if (nade.weapon == WeaponId::SmokeGrenade) {
        SmokeCloud* cloud = nullptr;
        for (auto& s : smokes_) {
            if (!s.active) {
                cloud = &s;
                break;
            }
        }
        if (!cloud && !smokes_.empty()) {
            cloud = &smokes_[0];
        }
        if (cloud) {
            cloud->active = true;
            cloud->pos = nade.pos;
            cloud->t = 0.0f;
            cloud->life = 12.0f;
            cloud->radius = 18.0f;
            cloud->emitAcc = 8.0f;
        }
    }
}

void World::updateBullets(float dt, Camera* cam) {
    for (auto& b : bullets_) {
        if (!b.active) {
            continue;
        }
        if (b.nade) {
            b.life -= dt;
            const Vec2 next = b.pos + b.vel * dt;
            const bool xHit = map_.solid(static_cast<int>(next.x / kTile), static_cast<int>(b.pos.y / kTile));
            const bool yHit = map_.solid(static_cast<int>(b.pos.x / kTile), static_cast<int>(next.y / kTile));
            if (xHit) {
                b.vel.x *= -0.62f;
                spawnParticle(particles_, b.pos, {}, {200, 200, 180, 255}, 0.12f, 3.0f);
            } else {
                b.pos.x = next.x;
            }
            if (yHit) {
                b.vel.y *= -0.62f;
                spawnParticle(particles_, b.pos, {}, {200, 200, 180, 255}, 0.12f, 3.0f);
            } else {
                b.pos.y = next.y;
            }
            b.vel = b.vel * std::max(0.0f, 1.0f - 0.55f * dt);
            b.pos.x = clamp(b.pos.x, 8.0f, map_.pixelW() - 8.0f);
            b.pos.y = clamp(b.pos.y, 8.0f, map_.pixelH() - 8.0f);
            if (mode_ == GameMode::ProtectBase) {
                bool onBase = false;
                for (const auto& base : bases_) {
                    if (base.active && base.hp > 0 && (base.pos - b.pos).length2() <= kBaseHitR * kBaseHitR) {
                        onBase = true;
                        break;
                    }
                }
                if (onBase) {
                    explodeGrenade(b, cam);
                    continue;
                }
            }
            if (b.life <= 0.0f) {
                explodeGrenade(b, cam);
            }
            continue;
        }
        b.life -= dt;
        b.pos += b.vel * dt;
        const int htx = static_cast<int>(b.pos.x / kTile);
        const int hty = static_cast<int>(b.pos.y / kTile);
        if (b.life <= 0.0f || map_.solid(htx, hty)) {
            if (map_.at(htx, hty) == Tile::Nuclear) {
                hitNuclear(htx, hty, b.owner, cam);
            }
            b.active = false;
            spawnParticle(particles_, b.pos, {}, {200, 200, 180, 255}, 0.15f, 3.0f);
            continue;
        }
        for (auto& a : actors_) {
            if (!a.active || !a.alive || a.id == b.owner) {
                continue;
            }
            if (b.owner >= 0 && b.owner < kMaxPlayers && actors_[b.owner].active) {
                if (!hostile(actors_[b.owner], a)) {
                    continue;
                }
            } else if (mode_ != GameMode::LastSurvivor && a.team == b.team) {
                continue;
            }
            if ((a.pos - b.pos).length2() <= kPlayerRadius * kPlayerRadius * 1.6f) {
                float dmg = b.damage;
                if (a.id == localId_) {
                    dmg *= tune(difficulty_).incomingMul;
                }
                a.hp -= static_cast<int>(dmg);
                b.active = false;
                spawnParticle(particles_, a.pos, b.vel * 0.05f, {180, 30, 30, 255}, 0.25f, 3.0f);
                if (a.hp <= 0) {
                    killActor(a);
                    if (b.owner >= 0 && b.owner < kMaxPlayers && actors_[b.owner].active) {
                        actors_[b.owner].kills++;
                    }
                }
                break;
            }
        }
        if (b.active && hitBases(b.pos, b.damage, b.owner, b.team)) {
            b.active = false;
            spawnParticle(particles_, b.pos, {}, {180, 30, 30, 255}, 0.18f, 3.0f);
        }
    }
}

void World::update(float dt, Input& input, Camera& cam, NetSession* net) {
    if (bannerT_ > 0.0f) {
        bannerT_ = std::max(0.0f, bannerT_ - dt);
    }
    if (paused_) {
        if (matchOver_ && net && net->role() == NetRole::Host) {
            NetSnapshot snap{};
            packSnapshot(snap);
            net->sendSnapshot(snap);
        }
        return;
    }
    tick_++;
    for (auto& b : bases_) {
        if (b.hurtT > 0.0f) {
            b.hurtT = std::max(0.0f, b.hurtT - dt);
        }
    }
    for (auto& a : actors_) {
        if (!a.active) {
            continue;
        }
        a.cooldown = std::max(0.0f, a.cooldown - dt);
        a.muzzleT = std::max(0.0f, a.muzzleT - dt);
        a.radarT = std::max(0.0f, a.radarT - dt);
        if (a.flashT > 0.0f) {
            a.flashT -= dt / std::max(0.01f, a.flashIntensity);
            if (a.flashT < 0.0f) {
                a.flashT = 0.0f;
            }
        }
        if (a.reloadT > 0.0f) {
            a.reloadT -= dt;
            if (a.reloadT <= 0.0f) {
                const WeaponDef& w = weaponDef(a.weapon);
                const int need = w.mag - a.mag;
                const int got = std::min(need, a.reserve);
                a.mag += got;
                a.reserve -= got;
            }
        }
        if (!a.alive) {
            if (mode_ == GameMode::LastSurvivor || mode_ == GameMode::SoloVsAll) {
                continue;
            }
            if (mode_ == GameMode::Zombie && a.zombie) {
                continue;
            }
            if (mode_ == GameMode::ProtectBase && !baseAlive(a.team)) {
                continue;
            }
            a.botThink += dt;
            if (a.botThink > tune(difficulty_).respawn) {
                a.botThink = 0.0f;
                respawn(a);
            }
        }
    }

    Actor* me = local();
    if (me && me->alive) {
        applyAnalog(*me, input.analogX(), input.analogY(), dt);
        moveActor(*me, dt);
        if (input.lShoulder()) {
            int w = static_cast<int>(me->weapon) - 1;
            if (w < 0) {
                w = static_cast<int>(WeaponId::Count) - 1;
            }
            me->weapon = static_cast<WeaponId>(w);
            const WeaponDef& def = weaponDef(me->weapon);
            me->mag = def.mag;
            me->reserve = def.reserve;
        }
        if (input.cycleWeapon() || input.pressed(SDL_CONTROLLER_BUTTON_Y)) {
            int w = (static_cast<int>(me->weapon) + 1) % static_cast<int>(WeaponId::Count);
            me->weapon = static_cast<WeaponId>(w);
            const WeaponDef& def = weaponDef(me->weapon);
            me->mag = def.mag;
            me->reserve = def.reserve;
        }
        if (input.reload() && me->reloadT <= 0.0f && me->mag < weaponDef(me->weapon).mag) {
            me->reloadT = weaponDef(me->weapon).reload;
        }
        const WeaponDef& w = weaponDef(me->weapon);
        if (w.automatic) {
            if (input.fireHeld()) {
                tryFire(*me, &cam);
            }
        } else if (input.firePressed()) {
            tryFire(*me, &cam);
        }
        cam.follow(me->pos, dt, map_.pixelW(), map_.pixelH());
    }
    camView_ = cam.view();
    camViewW_ = cam.viewW();
    camViewH_ = cam.viewH();

    if (!net || net->role() != NetRole::Client) {
        updateBots(dt);
        updateBullets(dt, &cam);
        updateLoot();
        updateWaves(dt);
        checkWinner();
    }

    for (auto& p : particles_) {
        if (!p.active) {
            continue;
        }
        p.life -= dt;
        p.pos += p.vel * dt;
        p.angle += p.spin * dt;
        const float drag = p.sprite == 2 ? 0.45f : 1.8f;
        p.vel = p.vel * (1.0f - drag * dt);
        if (p.life <= 0.0f) {
            p.active = false;
        }
    }
    for (auto& b : blasts_) {
        if (!b.active) {
            continue;
        }
        b.t += dt;
        if (b.t >= b.life) {
            b.active = false;
        }
    }
    for (auto& s : smokes_) {
        if (!s.active) {
            continue;
        }
        s.t += dt;
        const float grow = clamp(s.t / 1.6f, 0.0f, 1.0f);
        s.radius = 18.0f + grow * 70.0f;
        if (s.t < s.life - 2.2f) {
            s.emitAcc += dt * 36.0f;
            while (s.emitAcc >= 1.0f) {
                s.emitAcc -= 1.0f;
                const float ang = frand(0.0f, kPi * 2.0f);
                const float spd = frand(0.48f, 1.59f) * 55.0f;
                const float life = frand(2.3f, 5.0f);
                const float sz = frand(2.2f, 4.1f);
                spawnFxParticle(particles_, s.pos + Vec2{frand(-6.0f, 6.0f), frand(-6.0f, 6.0f)},
                                {std::cos(ang) * spd, std::sin(ang) * spd}, 2, life, sz, sz,
                                {168, 168, 170, 255}, {133, 133, 135, 0}, frand(-8.0f, 8.0f));
            }
        }
        if (s.t >= s.life) {
            s.active = false;
        }
    }
    for (auto& s : scorches_) {
        if (!s.active) {
            continue;
        }
        s.life -= dt;
        if (s.life <= 0.0f) {
            s.active = false;
        }
    }

    if (net && net->role() == NetRole::Host) {
        netAcc_ += dt;
        if (netAcc_ > 1.0f / 20.0f) {
            netAcc_ = 0.0f;
            NetSnapshot snap{};
            packSnapshot(snap);
            net->sendSnapshot(snap);
        }
        NetInput in{};
        while (net->takeInput(in)) {
            if (in.id < kMaxPlayers && actors_[in.id].active && !actors_[in.id].bot) {
                Actor& a = actors_[in.id];
                applyAnalog(a, in.ax / 127.0f, in.ay / 127.0f, dt);
                moveActor(a, dt);
                a.weapon = static_cast<WeaponId>(in.weapon % static_cast<uint8_t>(WeaponId::Count));
                if (in.buttons & NetSession::kBtnFire) {
                    tryFire(a, nullptr);
                }
                if (in.buttons & NetSession::kBtnReload) {
                    a.reloadT = weaponDef(a.weapon).reload;
                }
            }
        }
    } else if (net && net->role() == NetRole::Client) {
        NetInput in{};
        in.id = static_cast<uint8_t>(localId_);
        in.ax = static_cast<int8_t>(clamp(input.analogX(), -1.0f, 1.0f) * 127.0f);
        in.ay = static_cast<int8_t>(clamp(input.analogY(), -1.0f, 1.0f) * 127.0f);
        if (input.fireHeld() || input.down(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
            in.buttons |= NetSession::kBtnFire;
        }
        if (input.reload()) {
            in.buttons |= NetSession::kBtnReload;
        }
        if (me) {
            in.weapon = static_cast<uint8_t>(me->weapon);
        }
        net->sendInput(in);
        NetSnapshot snap{};
        if (net->takeSnapshot(snap)) {
            wave_ = snap.wave;
            matchOver_ = (snap.extra & 1) != 0;
            winnerId_ = static_cast<int>((snap.extra >> 1) & 0x3F) - 1;
            if (mode_ == GameMode::ProtectBase) {
                if (snap.teamCount >= 2 && snap.teamCount <= kMaxTeams) {
                    teamCount_ = snap.teamCount;
                }
                for (int i = 0; i < kMaxTeams; ++i) {
                    if (bases_[i].maxHp > 0) {
                        bases_[i].hp = snap.baseHp[i] * bases_[i].maxHp / 255;
                    }
                }
            }
            if (matchOver_) {
                paused_ = true;
            }
            for (int i = 0; i < snap.count; ++i) {
                const auto& s = snap.actors[i];
                if (s.id >= kMaxPlayers) {
                    continue;
                }
                Actor& a = actors_[s.id];
                a.active = true;
                a.id = s.id;
                a.team = static_cast<Team>(s.team);
                a.hp = s.hp;
                a.alive = (s.flags & 1) != 0;
                a.zombie = (s.flags & 2) != 0;
                a.weapon = static_cast<WeaponId>(s.weapon);
                if (a.id != localId_) {
                    a.pos = {s.x / 10.0f, s.y / 10.0f};
                    a.angle = rad(s.angle / 10.0f);
                }
                a.skinId = (s.flags >> 2) & 7;
                a.skin = a.zombie ? "zoimbie1" : actorSkin(a.team, a.id);
            }
        }
    }
}

std::string World::poseSprite(const Actor& a) const {
    const char* pose = weaponDef(a.weapon).pose;
    if (a.reloadT > 0.0f) {
        pose = "reload";
    }
    if (!a.alive) {
        pose = "stand";
    }
    return a.skin + "_" + pose;
}

void World::render(SDL_Renderer* r, Assets& assets, Camera& cam) {
    const Vec2 v = cam.view();
    const float z = cam.zoom;
    const float vw = cam.viewW();
    const float vh = cam.viewH();
    const int x0 = std::max(0, static_cast<int>(v.x / kTile) - 1);
    const int y0 = std::max(0, static_cast<int>(v.y / kTile) - 1);
    const int x1 = std::min(map_.width(), static_cast<int>((v.x + vw) / kTile) + 2);
    const int y1 = std::min(map_.height(), static_cast<int>((v.y + vh) / kTile) + 2);
    const std::string floor = map_.floorSprite();
    const std::string wall = map_.wallSprite();
    const std::string alt = map_.floorAltSprite();
    const Sprite* sprFloor = assets.get(floor);
    const Sprite* sprWall = assets.get(wall);
    const Sprite* sprAlt = assets.get(alt);
    const Sprite* sprCrate = assets.get(map_.csTile("crate"));
    const Sprite* sprBarrel = assets.get(map_.csTile("barrel"));
    const Sprite* sprCover = assets.get(map_.csTile("cover"));
    const Sprite* sprTree = assets.get(map_.csTile("tree"));
    const Sprite* sprDoor = assets.get(map_.csTile("door"));
    const Sprite* sprNuke = assets.get("cs_nuclear");
    const int overlap = 1;

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const Tile t = map_.at(x, y);
            const int px = static_cast<int>(std::floor((x * kTile - v.x) * z));
            const int py = static_cast<int>(std::floor((y * kTile - v.y) * z));
            const int px2 = static_cast<int>(std::ceil(((x + 1) * kTile - v.x) * z)) + overlap;
            const int py2 = static_cast<int>(std::ceil(((y + 1) * kTile - v.y) * z)) + overlap;
            const int tw = std::max(1, px2 - px);
            const int th = std::max(1, py2 - py);
            const Sprite* spr = sprFloor;
            if (t == Tile::Wall) {
                spr = sprWall;
            } else if (t == Tile::SiteA || t == Tile::SiteB) {
                spr = sprAlt;
            }
            assets.drawFit(r, spr, px, py, tw, th);
            if (t == Tile::Crate) {
                assets.drawFit(r, sprCrate, px, py, tw, th);
            } else if (t == Tile::Barrel) {
                assets.drawFit(r, sprBarrel, px, py, tw, th);
            } else if (t == Tile::Cover) {
                assets.drawFit(r, sprCover, px, py, tw, th);
            } else if (t == Tile::Tree) {
                assets.drawFit(r, sprTree, px, py, tw, th);
            } else if (t == Tile::Door) {
                assets.drawFit(r, sprDoor, px, py, tw, th);
            } else if (t == Tile::Nuclear) {
                assets.drawFit(r, sprNuke, px, py, tw, th);
            } else if (t == Tile::Water) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 40, 90, 180, 110);
                SDL_Rect wr{px, py, tw, th};
                SDL_RenderFillRect(r, &wr);
            }
        }
    }

    if (mode_ == GameMode::ProtectBase) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (const auto& b : bases_) {
            if (!b.active || b.hp <= 0) {
                continue;
            }
            const Vec2 p = cam.toScreen(b.pos.x, b.pos.y);
            const int sz = std::max(10, static_cast<int>(kTile * z));
            const bool hurt = b.hurtT > 0.0f;
            const Actor* me = local();
            const SDL_Color c = baseHudColor(b.team, me ? me->team : b.team);
            if (hurt) {
                SDL_SetRenderDrawColor(r, 255, 80, 40, 200);
            } else {
                SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 160);
            }
            SDL_Rect pad{static_cast<int>(p.x) - sz / 2, static_cast<int>(p.y) - sz / 2, sz, sz};
            SDL_RenderFillRect(r, &pad);
            assets.drawFit(r, map_.csTile("crate"), pad.x, pad.y, pad.w, pad.h);
            const int bw = std::max(18, static_cast<int>(40.0f * z));
            const int bh = std::max(3, static_cast<int>(5.0f * z));
            SDL_Rect bg{static_cast<int>(p.x) - bw / 2, pad.y - bh - 3, bw, bh};
            SDL_SetRenderDrawColor(r, 20, 20, 20, 230);
            SDL_RenderFillRect(r, &bg);
            const float ratio = b.maxHp > 0 ? clamp(static_cast<float>(b.hp) / static_cast<float>(b.maxHp), 0.0f, 1.0f) : 0.0f;
            bg.w = std::max(1, static_cast<int>(static_cast<float>(bw) * ratio));
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(r, &bg);
            assets.drawText(r, teamLetter(b.team), static_cast<int>(p.x) - 4, pad.y - bh - 16, c, false);
        }
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (auto& s : scorches_) {
        if (!s.active) {
            continue;
        }
        const float a = clamp(s.life / 8.0f, 0.0f, 1.0f);
        const Vec2 p = cam.toScreen(s.pos.x, s.pos.y);
        assets.drawFx(r, "cs_scorch", p.x, p.y, 0.0f, 1.15f * z, static_cast<int>(140.0f * a),
                      {20, 18, 14, 255}, false);
    }

    for (auto& b : bullets_) {
        if (!b.active) {
            continue;
        }
        if (!cam.onScreen(b.pos.x - 40.0f, b.pos.y - 40.0f, 80.0f, 80.0f)) {
            continue;
        }
        if (b.nade) {
            const Vec2 p = cam.toScreen(b.pos.x, b.pos.y);
            assets.drawCentered(r, weaponGroundSprite(b.weapon), p.x, p.y,
                                static_cast<float>(tick_ * 12), 0.9f * z);
        } else {
            drawBulletTracer(r, cam, b);
        }
    }

    for (auto& a : actors_) {
        if (!a.active || !a.alive) {
            continue;
        }
        if (!cam.onScreen(a.pos.x - 40.0f, a.pos.y - 40.0f, 80.0f, 80.0f)) {
            continue;
        }
        const Vec2 p = cam.toScreen(a.pos.x, a.pos.y);
        if (a.zombie) {
            assets.drawCentered(r, poseSprite(a), p.x, p.y, deg(a.angle), 0.55f * z);
            if (a.muzzleT > 0.0f) {
                assets.drawCentered(r, "cs_muzzle_0", p.x + std::cos(a.angle) * 12.0f * z,
                                    p.y + std::sin(a.angle) * 12.0f * z, deg(a.angle), 0.45f * z);
            }
        } else {
            drawCspspPlayer(r, assets, a, p.x, p.y, z);
        }
        if (a.alive) {
            SDL_Rect bg{static_cast<int>(p.x - 12 * z), static_cast<int>(p.y - 20 * z), static_cast<int>(24 * z),
                        std::max(1, static_cast<int>(3 * z))};
            SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
            SDL_RenderFillRect(r, &bg);
            SDL_SetRenderDrawColor(r, a.hp > 40 ? 40 : 180, a.hp > 40 ? 180 : 40, 40, 255);
            bg.w = std::max(1, static_cast<int>(a.hp * 24 * z / 100));
            SDL_RenderFillRect(r, &bg);
        }
    }

    for (auto& l : loot_) {
        if (!l.active) {
            continue;
        }
        if (!cam.onScreen(l.pos.x - 16.0f, l.pos.y - 16.0f, 32.0f, 32.0f)) {
            continue;
        }
        const Vec2 p = cam.toScreen(l.pos.x, l.pos.y);
        const float bob = std::sin(static_cast<float>(tick_) * 0.12f) * 2.0f * z;
        assets.drawCentered(r, weaponGroundSprite(l.weapon), p.x, p.y + bob, 0.0f, 1.0f * z);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (auto& fx : blasts_) {
        if (!fx.active) {
            continue;
        }
        const float u = clamp(fx.t / std::max(0.05f, fx.life), 0.0f, 1.0f);
        const Vec2 p = cam.toScreen(fx.pos.x, fx.pos.y);
        if (fx.kind == 1) {
            assets.drawFx(r, "cs_explosion", p.x, p.y, fx.t * 40.0f, (1.4f + u * 1.6f) * z,
                          static_cast<int>(220 * (1.0f - u)), {255, 90, 20, 255}, true);
        } else if (fx.kind == 0) {
            assets.drawFx(r, "cs_flash", p.x, p.y, 0.0f, (1.6f + u * 2.4f) * z,
                          static_cast<int>(230 * (1.0f - u)), {255, 255, 255, 255}, true);
        }
    }

    for (auto& p : particles_) {
        if (!p.active || p.sprite < 0) {
            continue;
        }
        const float u = p.maxLife > 0.0f ? clamp(1.0f - p.life / p.maxLife, 0.0f, 1.0f) : 1.0f;
        const Vec2 s = cam.toScreen(p.pos.x, p.pos.y);
        const float sz = (p.size + (p.sizeEnd - p.size) * u) * z;
        SDL_Color tint{
            static_cast<Uint8>(p.color.r + static_cast<int>((p.colorEnd.r - p.color.r) * u)),
            static_cast<Uint8>(p.color.g + static_cast<int>((p.colorEnd.g - p.color.g) * u)),
            static_cast<Uint8>(p.color.b + static_cast<int>((p.colorEnd.b - p.color.b) * u)),
            255};
        const int alpha = static_cast<int>(p.color.a + (p.colorEnd.a - p.color.a) * u);
        const char* name = p.sprite == 1 ? "cs_flash" : (p.sprite == 2 ? "cs_smoke" : "cs_explosion");
        assets.drawFx(r, name, s.x, s.y, p.angle, sz, alpha, tint, p.sprite != 2);
        if (p.sprite != 2) {
            assets.drawFx(r, name, s.x, s.y, p.angle, sz, alpha / 2, tint, true);
        }
    }

    for (auto& p : particles_) {
        if (!p.active || p.sprite >= 0) {
            continue;
        }
        const Vec2 s = cam.toScreen(p.pos.x, p.pos.y);
        const float fade = p.maxLife > 0.0f ? clamp(p.life / p.maxLife, 0.0f, 1.0f) : 1.0f;
        SDL_SetRenderDrawColor(r, p.color.r, p.color.g, p.color.b, static_cast<Uint8>(220 * fade));
        const int sz = std::max(1, static_cast<int>(p.size * z * (0.6f + 0.4f * fade)));
        SDL_Rect rc{static_cast<int>(s.x), static_cast<int>(s.y), sz, sz};
        SDL_RenderFillRect(r, &rc);
    }
}

void World::renderHud(SDL_Renderer* r, Assets& assets) {
    Actor* me = local();
    if (!me) {
        return;
    }
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect bar{0, kScreenH - 28, kScreenW, 28};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(r, &bar);

    const WeaponDef& w = weaponDef(me->weapon);
    const SDL_Color hud{240, 230, 180, 255};
    assets.drawText(r, "HP", 8, kScreenH - 22, hud, false);
    assets.drawInt(r, me->hp, 26, kScreenH - 22, hud, false);
    assets.drawText(r, w.name, 64, kScreenH - 22, hud, false);
    int ax = 64 + assets.textWidth(w.name, false) + 8;
    if (!w.melee) {
        assets.drawInt(r, me->mag, ax, kScreenH - 22, hud, false);
        assets.drawInt(r, me->reserve, ax + 28, kScreenH - 22, hud, false);
        ax += 56;
    }
    assets.drawText(r, "K", ax, kScreenH - 22, hud, false);
    assets.drawInt(r, me->kills, ax + 12, kScreenH - 22, hud, false);
    assets.drawText(r, "D", ax + 40, kScreenH - 22, hud, false);
    assets.drawInt(r, me->deaths, ax + 52, kScreenH - 22, hud, false);
    assets.drawHotspot(r, weaponGroundSprite(me->weapon), static_cast<float>(kScreenW - 22),
                       static_cast<float>(kScreenH - 14), -90.0f, 1.0f, 16.0f, 16.0f, false);

    if (mode_ == GameMode::Zombie) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
        SDL_Rect wb{6, 6, 118, 20};
        SDL_RenderFillRect(r, &wb);
        assets.drawText(r, "WAVE", 12, 10, {255, 80, 70, 255}, false);
        assets.drawInt(r, wave_, 52, 10, {255, 220, 80, 255}, false);
        if (wavePause_ > 0.0f) {
            assets.drawText(r, "NEXT", 78, 10, {200, 200, 180, 255}, false);
        }
    } else if (mode_ == GameMode::LastSurvivor) {
        int ids[kMaxPlayers];
        int n = 0;
        for (int i = 0; i < kMaxPlayers; ++i) {
            if (actors_[i].active) {
                ids[n++] = i;
            }
        }
        std::sort(ids, ids + n, [&](int a, int b) {
            if (actors_[a].alive != actors_[b].alive) {
                return actors_[a].alive && !actors_[b].alive;
            }
            if (actors_[a].kills != actors_[b].kills) {
                return actors_[a].kills > actors_[b].kills;
            }
            return actors_[a].id < actors_[b].id;
        });
        const int cols = n > 16 ? 2 : 1;
        const int rows = (n + cols - 1) / std::max(1, cols);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
        SDL_Rect wb{6, 6, cols * 150, 16 + rows * 11};
        SDL_RenderFillRect(r, &wb);
        assets.drawText(r, "ALIVE", 10, 8, {255, 220, 80, 255}, false);
        assets.drawInt(r, aliveCount(), 56, 8, {255, 220, 80, 255}, false);
        for (int i = 0; i < n; ++i) {
            const Actor& a = actors_[ids[i]];
            const int col = i / std::max(1, rows);
            const int row = i % std::max(1, rows);
            const int x = 10 + col * 150;
            const int y = 20 + row * 11;
            const SDL_Color colr = a.id == localId_ ? SDL_Color{255, 220, 70, 255}
                                                    : (a.alive ? SDL_Color{220, 220, 210, 255}
                                                               : SDL_Color{160, 160, 150, 255});
            if (!a.alive) {
                drawSkull(r, x, y, {200, 200, 190, 255});
            }
            assets.drawText(r, a.name, x + (a.alive ? 0 : 10), y, colr, false);
        }
    } else if (mode_ == GameMode::ProtectBase) {
        auto drawBaseBar = [&](int x, int y, const Base& b) {
            const SDL_Color col = baseHudColor(b.team, me->team);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
            SDL_Rect wb{x, y, 118, 16};
            SDL_RenderFillRect(r, &wb);
            assets.drawText(r, teamLetter(b.team), x + 3, y + 2, col, false);
            SDL_Rect bg{x + 16, y + 4, 70, 8};
            SDL_SetRenderDrawColor(r, 30, 30, 28, 255);
            SDL_RenderFillRect(r, &bg);
            const float ratio = b.maxHp > 0 ? clamp(static_cast<float>(std::max(0, b.hp)) / static_cast<float>(b.maxHp), 0.0f, 1.0f)
                                            : 0.0f;
            bg.w = std::max(1, static_cast<int>(70.0f * ratio));
            if (b.hurtT > 0.0f) {
                SDL_SetRenderDrawColor(r, 255, 80, 40, 255);
            } else {
                SDL_SetRenderDrawColor(r, col.r, col.g, col.b, 255);
            }
            SDL_RenderFillRect(r, &bg);
        };
        int shown = 0;
        int aliveBases = 0;
        for (int i = 0; i < teamCount_; ++i) {
            if (bases_[i].active && bases_[i].hp > 0) {
                ++aliveBases;
            }
        }
        const int cols = aliveBases <= 3 ? std::max(1, aliveBases) : 3;
        for (int i = 0; i < teamCount_; ++i) {
            if (!bases_[i].active || bases_[i].hp <= 0) {
                continue;
            }
            const int col = shown % cols;
            const int row = shown / cols;
            drawBaseBar(6 + col * 122, 6 + row * 17, bases_[i]);
            ++shown;
        }
    } else if (mode_ == GameMode::Normal) {
        int ids[kMaxPlayers];
        int n = 0;
        for (int i = 0; i < kMaxPlayers; ++i) {
            if (actors_[i].active) {
                ids[n++] = i;
            }
        }
        std::sort(ids, ids + n, [&](int a, int b) {
            if (actors_[a].kills != actors_[b].kills) {
                return actors_[a].kills > actors_[b].kills;
            }
            if (actors_[a].deaths != actors_[b].deaths) {
                return actors_[a].deaths < actors_[b].deaths;
            }
            return actors_[a].id < actors_[b].id;
        });
        const int show = std::min(8, n);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
        SDL_Rect wb{6, 6, 148, 14 + show * 11};
        SDL_RenderFillRect(r, &wb);
        assets.drawText(r, "TOP", 10, 8, {200, 200, 180, 255}, false);
        for (int i = 0; i < show; ++i) {
            const Actor& a = actors_[ids[i]];
            const SDL_Color colr = a.id == localId_ ? SDL_Color{255, 220, 70, 255} : SDL_Color{220, 220, 210, 255};
            assets.drawText(r, a.name, 10, 20 + i * 11, colr, false);
            assets.drawInt(r, a.kills, 110, 20 + i * 11, colr, false);
        }
    }

    const int hudRows = mode_ == GameMode::ProtectBase ? std::max(1, (teamCount_ + 2) / 3) : 1;
    const int modeY = mode_ == GameMode::ProtectBase ? (8 + hudRows * 17)
                      : (mode_ == GameMode::Normal || mode_ == GameMode::LastSurvivor ? -1 : 28);
    if (modeY >= 0) {
        assets.drawText(r, gameModeName(mode_), 6, modeY, {160, 170, 150, 255}, false);
    }

    constexpr int mw = 64;
    constexpr int mh = 64;
    bakeMinimap(r);
    SDL_Rect mm{kScreenW - mw - 6, 6, mw, mh};
    if (miniTex_) {
        SDL_RenderCopy(r, miniTex_, nullptr, &mm);
    } else {
        SDL_SetRenderDrawColor(r, 10, 12, 10, 180);
        SDL_RenderFillRect(r, &mm);
    }
    const float sx = static_cast<float>(mw) / static_cast<float>(std::max(1, map_.width()));
    const float sy = static_cast<float>(mh) / static_cast<float>(std::max(1, map_.height()));
    if (mode_ == GameMode::ProtectBase) {
        for (const auto& b : bases_) {
            if (!b.active || b.hp <= 0) {
                continue;
            }
            const int mx = mm.x + static_cast<int>(b.pos.x / kTile * sx);
            const int my = mm.y + static_cast<int>(b.pos.y / kTile * sy);
            const SDL_Color c = baseHudColor(b.team, me->team);
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
            SDL_Rect p{mx - 2, my - 2, 5, 5};
            SDL_RenderFillRect(r, &p);
        }
    }
    for (auto& a : actors_) {
        if (!a.active || !a.alive) {
            continue;
        }
        const int mx = mm.x + static_cast<int>(a.pos.x / kTile * sx);
        const int my = mm.y + static_cast<int>(a.pos.y / kTile * sy);
        if (a.id == localId_) {
            SDL_SetRenderDrawColor(r, 40, 220, 70, 255);
            SDL_Rect p{mx - 2, my - 2, 5, 5};
            SDL_RenderFillRect(r, &p);
            continue;
        }
        if (a.zombie) {
            continue;
        }
        if (me && !hostile(*me, a)) {
            SDL_SetRenderDrawColor(r, 70, 140, 255, 255);
            SDL_Rect p{mx - 1, my - 1, 3, 3};
            SDL_RenderFillRect(r, &p);
            continue;
        }
        if (mode_ == GameMode::LastSurvivor) {
            SDL_SetRenderDrawColor(r, 230, 50, 50, 255);
            SDL_Rect p{mx - 1, my - 1, 3, 3};
            SDL_RenderFillRect(r, &p);
            continue;
        }
        if (a.radarT <= 0.0f) {
            continue;
        }
        SDL_SetRenderDrawColor(r, 230, 50, 50, 255);
        SDL_Rect p{mx - 1, my - 1, 3, 3};
        SDL_RenderFillRect(r, &p);
    }

    if (me->reloadT > 0.0f) {
        assets.drawText(r, "RELOADING", kScreenW / 2 - 36, kScreenH / 2 + 40, {255, 220, 80, 255}, false);
    }

    bool inSmoke = false;
    for (auto& s : smokes_) {
        if (s.active && (me->pos - s.pos).length() < s.radius * 0.75f) {
            inSmoke = true;
            break;
        }
    }
    if (inSmoke && me->alive) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 90, 90, 88, 90);
        SDL_Rect full{0, 0, kScreenW, kScreenH};
        SDL_RenderFillRect(r, &full);
    }
    if (me->alive && me->flashT > 0.0f) {
        float alpha = me->flashT >= 10.0f ? 1.0f : me->flashT / 10.0f;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(alpha * 255.0f));
        SDL_Rect full{0, 0, kScreenW, kScreenH};
        SDL_RenderFillRect(r, &full);
    }
    if (bannerT_ > 0.0f && banner_[0]) {
        const float fade = bannerT_ > 0.7f ? 1.0f : bannerT_ / 0.7f;
        const int tw = assets.textWidth(banner_, false);
        const int x = std::max(8, kScreenW / 2 - tw / 2);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<Uint8>(190 * fade));
        SDL_Rect bb{x - 8, 118, tw + 16, 18};
        SDL_RenderFillRect(r, &bb);
        assets.drawText(r, banner_, x, 122, {255, 220, 80, static_cast<Uint8>(255 * fade)}, false);
    }
}
