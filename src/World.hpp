#pragma once

#include "Assets.hpp"
#include "Camera.hpp"
#include "Input.hpp"
#include "Map.hpp"
#include "Net.hpp"
#include "Types.hpp"
#include "Weapons.hpp"

#include <string>
#include <vector>

struct Actor {
    int id = 0;
    bool active = false;
    bool alive = false;
    bool bot = false;
    Team team = Team::Terrorist;
    Vec2 pos;
    Vec2 vel;
    float angle = 0.0f;
    int hp = 100;
    WeaponId weapon = WeaponId::Glock;
    int mag = 20;
    int reserve = 120;
    float cooldown = 0.0f;
    float reloadT = 0.0f;
    float muzzleT = 0.0f;
    float botThink = 0.0f;
    float radarT = 0.0f;
    Vec2 botTarget;
    Vec2 botSteer;
    Vec2 botLast;
    float botLockT = 0.0f;
    float botStuckT = 0.0f;
    float botPathT = 0.0f;
    float botGoalD = 1.0e9f;
    int botWayX = -1;
    int botWayY = -1;
    int botPrevX = -1;
    int botPrevY = -1;
    int botWallDir = 1;
    int botFail = 0;
    int botJob = 0;
    int botBuddy = -1;
    float botJobT = 0.0f;
    Vec2 botHome;
    int botFocus = -1;
    Vec2 botFocusPos;
    bool botFire = false;
    bool botHold = false;
    int kills = 0;
    int deaths = 0;
    bool zombie = false;
    int skinId = 0;
    float walkT = 0.0f;
    float flashT = 0.0f;
    float flashIntensity = 1.0f;
    std::string skin = "manBrown";
    std::string name = "Player";
};

struct Bullet {
    bool active = false;
    Vec2 pos;
    Vec2 origin;
    Vec2 vel;
    float life = 0.0f;
    float damage = 0.0f;
    int owner = -1;
    Team team = Team::Terrorist;
    bool nade = false;
    WeaponId weapon = WeaponId::Glock;
};

struct Particle {
    bool active = false;
    Vec2 pos;
    Vec2 vel;
    float life = 0.0f;
    float maxLife = 0.2f;
    SDL_Color color{255, 200, 80, 255};
    float size = 2.0f;
    int sprite = -1;
    float angle = 0.0f;
    float spin = 0.0f;
    float sizeEnd = 2.0f;
    SDL_Color colorEnd{255, 200, 80, 0};
};

struct BlastFx {
    bool active = false;
    Vec2 pos;
    float t = 0.0f;
    float life = 0.45f;
    int kind = 0;
};

struct SmokeCloud {
    bool active = false;
    Vec2 pos;
    float t = 0.0f;
    float life = 12.0f;
    float radius = 0.0f;
    float emitAcc = 0.0f;
};

struct Scorch {
    bool active = false;
    Vec2 pos;
    float life = 8.0f;
};

struct Loot {
    bool active = false;
    Vec2 pos;
    WeaponId weapon = WeaponId::Glock;
    int ammo = 0;
};

struct NukeBlock {
    bool active = false;
    int tx = 0;
    int ty = 0;
    int hp = 0;
};

struct Base {
    bool active = false;
    Team team = Team::Terrorist;
    Vec2 pos;
    int hp = 0;
    int maxHp = 0;
    float hurtT = 0.0f;
};

class World {
public:
    ~World() { freeMinimap(); }
    bool loadMap(const std::string& path);
    void startMatch(Team localTeam, int botCount, int localId = 0, Difficulty difficulty = Difficulty::Medium,
                    GameMode mode = GameMode::Normal, int teamCount = 2, int localSkin = -1);
    void addPlayer(int id, Team team, bool bot, const std::string& name);
    void fillBots(int count);
    void update(float dt, Input& input, Camera& cam, NetSession* net);
    void render(SDL_Renderer* r, Assets& assets, Camera& cam);
    void renderHud(SDL_Renderer* r, Assets& assets);

    GameMap& map() { return map_; }
    const GameMap& map() const { return map_; }
    Actor* local() { return localId_ >= 0 && localId_ < kMaxPlayers ? &actors_[localId_] : nullptr; }
    const Actor* local() const { return localId_ >= 0 && localId_ < kMaxPlayers ? &actors_[localId_] : nullptr; }
    const Actor* actor(int id) const {
        return id >= 0 && id < kMaxPlayers ? &actors_[id] : nullptr;
    }
    int localId() const { return localId_; }
    Difficulty difficulty() const { return difficulty_; }
    GameMode mode() const { return mode_; }
    int teamCount() const { return teamCount_; }
    int wave() const { return wave_; }
    bool matchOver() const { return matchOver_; }
    int winnerId() const { return winnerId_; }
    int aliveCount() const;
    bool paused() const { return paused_; }
    void setPaused(bool p) { paused_ = p; }

private:
    void spawnActor(int id, Team team, bool bot, const std::string& name, bool zombie = false);
    void rollTeamSkins(int localTeamIdx, int localSkin);
    int takeSkin(Team team, bool zombie);
    Vec2 zombieEdgeSpawn(int id);
    bool nearHumanSpawn(const Vec2& p) const;
    bool onVisibleScreen(const Vec2& p) const;
    void applyAnalog(Actor& a, float ax, float ay, float dt, bool faceMove = true);
    void tryFire(Actor& a, Camera* cam);
    void updateBots(float dt);
    void updateBullets(float dt, Camera* cam);
    void explodeGrenade(Bullet& nade, Camera* cam);
    void receiveFlash(Actor& a, float intensity);
    bool throughSmoke(Vec2 a, Vec2 b) const;
    bool sightBlocked(Vec2 a, Vec2 b, float maxDist = 0.0f) const;
    void emitBurst(Vec2 pos, int n, float speed, SDL_Color c, float life, float size);
    void emitNadeParticles(Vec2 pos, int kind);
    void scanNukes();
    void hitNuclear(int tx, int ty, int owner, Camera* cam);
    void explodeNuclear(NukeBlock& n, int owner, Camera* cam);
    void updateWaves(float dt);
    void spawnWave(int n);
    int findSlot() const;
    void respawn(Actor& a);
    void moveActor(Actor& a, float dt);
    bool tileOpen(int tx, int ty) const;
    Vec2 tileCenter(int tx, int ty) const;
    bool refreshBotPath(Actor& a, Vec2 goal);
    void recoverBot(Actor& a, Vec2 goal);
    void wanderBot(Actor& a);
    void setBotWay(Actor& a, int tx, int ty);
    Vec2 botSlide(const Actor& a, Vec2 want) const;
    void assignBotJobs();
    Vec2 pickPatrolPoint(const Actor& a) const;
    Vec2 pickDefendPoint(Team team) const;
    Vec2 spawnAtBase(Team team, int slot) const;
    Vec2 basePos(Team team) const;
    bool baseAlive(Team team) const;
    Team huntBaseTeam(Team from, Vec2 pos) const;
    void setupBases();
    void hurtBase(Team victim, float dmg, int owner);
    bool hitBases(Vec2 pos, float dmg, int owner, Team ownerTeam);
    void packSnapshot(NetSnapshot& snap) const;
    void unstickActor(Actor& a);
    bool hostile(const Actor& a, const Actor& b) const;
    void checkWinner();
    std::string poseSprite(const Actor& a) const;
    Actor* nearestEnemy(const Actor& a);
    void killActor(Actor& a);
    void dropLoot(Vec2 pos, WeaponId weapon, int ammo);
    void applyLoot(Actor& a, Loot& loot);
    void updateLoot();
    void bakeMinimap(SDL_Renderer* r);
    void freeMinimap();

    GameMap map_;
    Actor actors_[kMaxPlayers];
    std::vector<Bullet> bullets_;
    std::vector<Particle> particles_;
    std::vector<Loot> loot_;
    std::vector<BlastFx> blasts_;
    std::vector<SmokeCloud> smokes_;
    std::vector<Scorch> scorches_;
    SDL_Texture* miniTex_ = nullptr;
    int miniGenW_ = 0;
    int miniGenH_ = 0;
    int localId_ = 0;
    Difficulty difficulty_ = Difficulty::Medium;
    GameMode mode_ = GameMode::Normal;
    int teamCount_ = 2;
    int teamSkin_[kMaxTeams]{};
    int localSkin_ = 0;
    int wave_ = 0;
    float wavePause_ = 0.0f;
    bool matchOver_ = false;
    int winnerId_ = -1;
    bool paused_ = false;
    uint16_t tick_ = 0;
    float netAcc_ = 0.0f;
    Vec2 camView_{};
    float camViewW_ = static_cast<float>(kScreenW);
    float camViewH_ = static_cast<float>(kScreenH);
    Base bases_[kMaxTeams]{};
    std::vector<NukeBlock> nukes_;
    char banner_[80]{};
    float bannerT_ = 0.0f;
};

void drawCspspSkin(SDL_Renderer* r, Assets& assets, int skinId, float sx, float sy, float z, float angle = -1.2f);
