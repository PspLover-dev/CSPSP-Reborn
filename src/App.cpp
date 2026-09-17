#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#ifdef PSP
#include <pspkernel.h>
#include <pspiofilemgr.h>
#endif

namespace {
const char* kMenu[] = {"SOLO", "MULTIPLAYER", "MAP EDITOR", "QUIT"};
const char* kMulti[] = {"HOST PARTY", "JOIN SCAN", "ENTER SERVER", "BACK"};
const char* kSizes[] = {"all", "small", "medium", "big", "extra"};
constexpr int kSizeFilterCount = 5;
const char* kEditTiles[] = {". floor", "# wall", "C crate", "B barrel", "D door", "S cover", "R tree", "T spawn-T",
                            "O spawn-CT", "A site-A", "X site-B"};
const Tile kEditTileIds[] = {Tile::Floor,  Tile::Wall,    Tile::Crate,   Tile::Barrel, Tile::Door, Tile::Cover,
                             Tile::Tree,   Tile::SpawnT,  Tile::SpawnCT, Tile::SiteA,  Tile::SiteB};
constexpr int kEditTileCount = 11;
constexpr int kModeCount = static_cast<int>(GameMode::Count);
const int kEditorDims[] = {50, 100, 200, 500};
const char* kEditorSizeIds[] = {"small", "medium", "big", "extra"};
const char* kEditorSizeLabels[] = {"NEW SMALL  50x50", "NEW MEDIUM  100x100", "NEW BIG  200x200", "NEW LARGE  500x500"};
const char* kCharset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

bool isSavedMap(const MapEntry& e) {
    return e.file.find("saved_") != std::string::npos || e.name.find("SAVED_") != std::string::npos;
}

int parseSavedNum(const std::string& s) {
    int n = 0;
    bool any = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            any = true;
            n = n * 10 + (c - '0');
        } else if (any) {
            break;
        }
    }
    return n;
}

int playerCountForSize(const std::string& size) {
    if (size == "small") {
        return 8;
    }
    if (size == "medium") {
        return 16;
    }
    if (size == "big") {
        return 24;
    }
    return 32;
}

const char* teamTag(int team, int teamCount, GameMode mode) {
    if (mode == GameMode::Zombie) {
        return "CT";
    }
    if (teamCount > 2 || mode == GameMode::ProtectBase) {
        return teamLetter(teamFromIndex(team));
    }
    return team == 0 ? "T" : "CT";
}

void clampTeamPicks(int& teamPick, int& teamCount, const std::string& size, GameMode mode) {
    if (mode == GameMode::Zombie) {
        teamCount = 2;
        teamPick = 1;
        return;
    }
    if (!modeUsesTeams(mode)) {
        teamCount = 1;
        teamPick = 0;
        return;
    }
    const int mx = maxTeamsForSize(size);
    teamCount = std::max(2, std::min(mx, teamCount));
    teamPick = ((teamPick % teamCount) + teamCount) % teamCount;
}

const char* sizeLabel(const std::string& size) {
    if (size == "extra") {
        return "large";
    }
    return size.c_str();
}

float zoomForSize(const std::string& size) {
    if (size == "big" || size == "extra" || size == "large") {
        return 16.0f;
    }
    return 32.0f;
}

bool existsDir(const std::string& p) {
    struct stat st {};
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string findData(const char* name) {
    std::vector<std::string> cands;
    if (char* base = SDL_GetBasePath()) {
        std::string b = base;
        SDL_free(base);
        cands.push_back(joinPath(b, name));
    }
    cands.push_back(name);
    cands.push_back(std::string("../") + name);
    cands.push_back(std::string("../../") + name);
#ifdef PSP
    cands.push_back(std::string("ms0:/PSP/GAME/CSPSP/") + name);
#endif
    for (const auto& c : cands) {
        if (existsDir(c)) {
            return c;
        }
    }
    return cands.empty() ? std::string(name) : cands.front();
}

void box(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color fill, SDL_Color edge) {
    SDL_Rect rc{x, y, w, h};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, edge.r, edge.g, edge.b, edge.a);
    SDL_RenderDrawRect(r, &rc);
}
} // namespace

bool App::init() {
#ifdef PSP
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "psp");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
        std::printf("SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    if (char* base = SDL_GetBasePath()) {
        chdir(base);
        SDL_free(base);
    }
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        std::printf("IMG_Init: %s\n", IMG_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        std::printf("TTF_Init: %s\n", TTF_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("CS 2D PSP", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kScreenW, kScreenH, 0);
    if (!window_) {
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }
    SDL_RenderSetLogicalSize(renderer_, kScreenW, kScreenH);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    gfxDir_ = findData("Gfx");
    mapsDir_ = findData("maps");
    ensureMapsDir();
    if (!assets_.load(renderer_, gfxDir_)) {
        std::printf("Failed to load assets from %s\n", gfxDir_.c_str());
        return false;
    }
    input_.init();
    maps_ = GameMap::loadIndex(mapsDir_);
    importSavedMaps();
    orderMaps();
    if (maps_.empty()) {
        status_ = "No maps in /maps";
    }
    return true;
}

void App::shutdown() {
    net_.shutdown();
    assets_.destroy();
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void App::run() {
    Uint32 last = SDL_GetTicks();
    running_ = true;
    while (running_) {
        input_.beginFrame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            input_.handleEvent(e);
        }
        input_.pollNative();
        if (input_.quitRequested()) {
            running_ = false;
        }
        const Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.05f) {
            dt = 0.05f;
        }
        update(dt);
        render();
    }
}

void App::goTo(Screen s) {
    screen_ = s;
    inputLock_ = 0.2f;
    assets_.resetDrawState(renderer_);
}

void App::update(float dt) {
    if (inputLock_ > 0.0f) {
        inputLock_ -= dt;
        if (screen_ == Screen::Play) {
            camera_.update(dt);
            world_.update(dt, input_, camera_, net_.role() == NetRole::Offline ? nullptr : &net_);
        }
        if (screen_ == Screen::Editor) {
            updateEditor(dt);
        }
        return;
    }
    switch (screen_) {
        case Screen::Menu:
            updateMenu();
            break;
        case Screen::Solo:
            updateSolo();
            break;
        case Screen::Mode:
            updateMode();
            break;
        case Screen::Difficulty:
            updateDifficulty();
            break;
        case Screen::Skin:
            updateSkin();
            break;
        case Screen::Multi:
            updateMulti();
            break;
        case Screen::Host:
            updateHost();
            break;
        case Screen::Join:
            updateJoin();
            break;
        case Screen::Enter:
            updateEnter();
            break;
        case Screen::Play:
            camera_.update(dt);
            updatePlay();
            world_.update(dt, input_, camera_, net_.role() == NetRole::Offline ? nullptr : &net_);
            if (net_.role() != NetRole::Offline) {
                net_.pump();
            }
            break;
        case Screen::EditorSize:
            updateEditorSize();
            break;
        case Screen::Editor:
            updateEditor(dt);
            break;
    }
}

void App::updateMenu() {
    if (input_.up()) {
        menuIndex_ = (menuIndex_ + 3) % 4;
    }
    if (input_.downNav()) {
        menuIndex_ = (menuIndex_ + 1) % 4;
    }
    if (input_.confirm()) {
        if (menuIndex_ == 0) {
            menuIndex_ = 0;
            refreshSavedMaps();
            goTo(Screen::Solo);
        } else if (menuIndex_ == 1) {
            menuIndex_ = 0;
            goTo(Screen::Multi);
        } else if (menuIndex_ == 2) {
            editPick_ = 0;
            refreshSavedMaps();
            goTo(Screen::EditorSize);
        } else {
            running_ = false;
        }
    }
}

void App::updateSolo() {
    if (input_.cancel() || input_.start()) {
        menuIndex_ = 0;
        goTo(Screen::Menu);
        return;
    }
    if (input_.lShoulder()) {
        soloFilter_ = (soloFilter_ + kSizeFilterCount - 1) % kSizeFilterCount;
    }
    if (input_.rShoulder()) {
        soloFilter_ = (soloFilter_ + 1) % kSizeFilterCount;
    }
    auto visible = [&](const MapEntry& e) {
        if (soloFilter_ == 0) {
            return true;
        }
        return e.size == kSizes[soloFilter_];
    };
    if (input_.up()) {
        do {
            soloMap_ = (soloMap_ + static_cast<int>(maps_.size()) - 1) % static_cast<int>(std::max<size_t>(1, maps_.size()));
        } while (!maps_.empty() && !visible(maps_[static_cast<size_t>(soloMap_)]));
    }
    if (input_.downNav()) {
        do {
            soloMap_ = (soloMap_ + 1) % static_cast<int>(std::max<size_t>(1, maps_.size()));
        } while (!maps_.empty() && !visible(maps_[static_cast<size_t>(soloMap_)]));
    }
    if (input_.left() || input_.right()) {
        int maxT = 2;
        if (!maps_.empty()) {
            maxT = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
        }
        teamPick_ = (teamPick_ + (input_.right() ? 1 : maxT - 1)) % maxT;
    }
    if (input_.confirm()) {
        if (maps_.empty()) {
            status_ = "No maps";
            return;
        }
        screen_ = Screen::Mode;
        hostModeSelect_ = false;
        modePick_ = 0;
        difficultyPick_ = 1;
        teamCountPick_ = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
    }
}

void App::updateMode() {
    if (input_.cancel() || input_.start()) {
        screen_ = hostModeSelect_ ? Screen::Multi : Screen::Solo;
        hostModeSelect_ = false;
        return;
    }
    if (input_.up()) {
        modePick_ = (modePick_ + kModeCount - 1) % kModeCount;
    }
    if (input_.downNav()) {
        modePick_ = (modePick_ + 1) % kModeCount;
    }
    if (!maps_.empty()) {
        clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                       static_cast<GameMode>(modePick_));
    }
    if (!maps_.empty() && modeUsesTeams(static_cast<GameMode>(modePick_))) {
        const int mx = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
        if (input_.left()) {
            teamCountPick_ = teamCountPick_ <= 2 ? mx : teamCountPick_ - 1;
        }
        if (input_.right()) {
            teamCountPick_ = teamCountPick_ >= mx ? 2 : teamCountPick_ + 1;
        }
        clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                       static_cast<GameMode>(modePick_));
    }
    if (input_.confirm()) {
        if (hostModeSelect_) {
            status_ = "Creating party...";
            if (net_.host(partyName_)) {
                screen_ = Screen::Host;
                status_ = "Hosting " + net_.partyName();
                net_.setLocalTeam(static_cast<uint8_t>(teamPick_));
                net_.sendLobbyInfo(static_cast<uint16_t>(soloMap_), static_cast<uint8_t>(modePick_),
                                   static_cast<uint8_t>(difficultyPick_), static_cast<uint8_t>(teamCountPick_));
            } else {
                status_ = net_.lastError();
                screen_ = Screen::Multi;
            }
            hostModeSelect_ = false;
        } else {
            screen_ = Screen::Difficulty;
        }
    }
}

void App::updateDifficulty() {
    if (input_.cancel() || input_.start()) {
        screen_ = Screen::Mode;
        return;
    }
    if (input_.up()) {
        difficultyPick_ = (difficultyPick_ + 2) % 3;
    }
    if (input_.downNav()) {
        difficultyPick_ = (difficultyPick_ + 1) % 3;
    }
    if (input_.confirm()) {
        if (modePicksTeamSkin(static_cast<GameMode>(modePick_))) {
            screen_ = Screen::Skin;
        } else {
            startSolo();
        }
    }
}

void App::updateSkin() {
    if (input_.cancel() || input_.start()) {
        screen_ = Screen::Difficulty;
        return;
    }
    if (input_.left()) {
        skinPick_ = (skinPick_ + kSkinCount - 1) % kSkinCount;
    }
    if (input_.right()) {
        skinPick_ = (skinPick_ + 1) % kSkinCount;
    }
    if (input_.up()) {
        skinPick_ = (skinPick_ + kSkinCount - 4) % kSkinCount;
    }
    if (input_.downNav()) {
        skinPick_ = (skinPick_ + 4) % kSkinCount;
    }
    if (input_.confirm()) {
        startSolo();
    }
}

bool App::startSolo() {
    if (maps_.empty()) {
        status_ = "No maps";
        return false;
    }
    const auto& e = maps_[static_cast<size_t>(soloMap_)];
    if (!world_.loadMap(joinPath(mapsDir_, e.file))) {
        status_ = "Map load failed";
        return false;
    }
    botCount_ = std::max(1, playerCountForSize(e.size) - 1);
    const GameMode mode = static_cast<GameMode>(modePick_);
    clampTeamPicks(teamPick_, teamCountPick_, e.size, mode);
    const int skin = modePicksTeamSkin(mode) ? skinPick_ : -1;
    world_.startMatch(teamFromIndex(teamPick_), botCount_, 0, static_cast<Difficulty>(difficultyPick_), mode,
                      teamCountPick_, skin);
    if (auto* me = world_.local()) {
        camera_.pos = {me->pos.x - camera_.viewW() * 0.5f, me->pos.y - camera_.viewH() * 0.5f};
    }
    screen_ = Screen::Play;
    status_.clear();
    return true;
}

void App::updateMulti() {
    if (input_.up()) {
        menuIndex_ = (menuIndex_ + 3) % 4;
    }
    if (input_.downNav()) {
        menuIndex_ = (menuIndex_ + 1) % 4;
    }
    if (input_.cancel()) {
        screen_ = Screen::Menu;
        menuIndex_ = 1;
        return;
    }
    if (!input_.confirm()) {
        return;
    }
    if (menuIndex_ == 0) {
        hostModeSelect_ = true;
        modePick_ = 0;
        difficultyPick_ = 1;
        if (!maps_.empty()) {
            teamCountPick_ = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
            clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size, GameMode::Normal);
        }
        screen_ = Screen::Mode;
        status_.clear();
    } else if (menuIndex_ == 1) {
        status_ = "Scanning...";
        net_.scan(scanResults_);
        scanIndex_ = 0;
        screen_ = Screen::Join;
        status_ = scanResults_.empty() ? "No parties found" : "Select a party";
    } else if (menuIndex_ == 2) {
        nameCursor_ = 0;
        screen_ = Screen::Enter;
    } else {
        screen_ = Screen::Menu;
        menuIndex_ = 1;
    }
}

void App::updateHost() {
    net_.pump();
    if (input_.cancel()) {
        net_.leave();
        screen_ = Screen::Multi;
        return;
    }
    if (input_.left() || input_.right()) {
        if (modeUsesTeams(static_cast<GameMode>(modePick_))) {
            if (!maps_.empty()) {
                clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                               static_cast<GameMode>(modePick_));
            }
            teamPick_ = (teamPick_ + (input_.right() ? 1 : teamCountPick_ - 1)) % std::max(2, teamCountPick_);
            net_.setLocalTeam(static_cast<uint8_t>(teamPick_));
        }
    }
    if (input_.select() && !maps_.empty() && modeUsesTeams(static_cast<GameMode>(modePick_))) {
        const int mx = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
        teamCountPick_ = teamCountPick_ >= mx ? 2 : teamCountPick_ + 1;
        clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                       static_cast<GameMode>(modePick_));
        net_.sendLobbyInfo(static_cast<uint16_t>(soloMap_), static_cast<uint8_t>(modePick_),
                           static_cast<uint8_t>(difficultyPick_), static_cast<uint8_t>(teamCountPick_));
    }
    if (input_.lShoulder()) {
        modePick_ = (modePick_ + kModeCount - 1) % kModeCount;
        if (!maps_.empty()) {
            clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                           static_cast<GameMode>(modePick_));
        }
        net_.sendLobbyInfo(static_cast<uint16_t>(soloMap_), static_cast<uint8_t>(modePick_),
                           static_cast<uint8_t>(difficultyPick_), static_cast<uint8_t>(teamCountPick_));
    }
    if (input_.rShoulder()) {
        modePick_ = (modePick_ + 1) % kModeCount;
        if (!maps_.empty()) {
            clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                           static_cast<GameMode>(modePick_));
        }
        net_.sendLobbyInfo(static_cast<uint16_t>(soloMap_), static_cast<uint8_t>(modePick_),
                           static_cast<uint8_t>(difficultyPick_), static_cast<uint8_t>(teamCountPick_));
    }
    if (input_.up() || input_.downNav()) {
        soloMap_ = (soloMap_ + (input_.downNav() ? 1 : -1) + static_cast<int>(maps_.size())) %
                   static_cast<int>(std::max<size_t>(1, maps_.size()));
        if (!maps_.empty()) {
            teamCountPick_ = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
            clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                           static_cast<GameMode>(modePick_));
        }
        net_.sendLobbyInfo(static_cast<uint16_t>(soloMap_), static_cast<uint8_t>(modePick_),
                           static_cast<uint8_t>(difficultyPick_), static_cast<uint8_t>(teamCountPick_));
    }
    if (input_.confirm() && net_.peerCount() >= 1) {
        net_.sendStart(static_cast<uint16_t>(soloMap_), static_cast<uint8_t>(modePick_),
                       static_cast<uint8_t>(difficultyPick_), static_cast<uint8_t>(teamCountPick_));
        startHostedMatch();
    }
}

bool App::startHostedMatch() {
    if (maps_.empty()) {
        return false;
    }
    const auto& e = maps_[static_cast<size_t>(soloMap_)];
    if (!world_.loadMap(joinPath(mapsDir_, e.file))) {
        return false;
    }
    clampTeamPicks(teamPick_, teamCountPick_, e.size, static_cast<GameMode>(modePick_));
    world_.startMatch(teamFromIndex(teamPick_), 0, 0, static_cast<Difficulty>(difficultyPick_),
                      static_cast<GameMode>(modePick_), teamCountPick_);
    net_.setLocalTeam(static_cast<uint8_t>(teamPick_));
    const GameMode mode = static_cast<GameMode>(modePick_);
    if (mode == GameMode::Zombie) {
        teamPick_ = 1;
        if (auto* me = world_.local()) {
            me->team = Team::Counter;
        }
    }
    int humans = 1;
    for (int i = 1; i < kMaxPlayers; ++i) {
        const NetPeer& p = net_.peers()[i];
        if (!p.connected) {
            continue;
        }
        Team pt = teamFromIndex(static_cast<int>(p.team) % std::max(2, teamCountPick_));
        if (mode == GameMode::Zombie) {
            pt = Team::Counter;
        } else if (!modeUsesTeams(mode)) {
            pt = Team::Terrorist;
        }
        world_.addPlayer(i, pt, false, p.name);
        ++humans;
    }
    if (mode == GameMode::Normal || mode == GameMode::SoloVsAll || mode == GameMode::ProtectBase ||
        mode == GameMode::LastSurvivor) {
        const int want = playerCountForSize(e.size);
        world_.fillBots(std::max(0, want - humans));
    }
    if (auto* me = world_.local()) {
        camera_.pos = {me->pos.x - camera_.viewW() * 0.5f, me->pos.y - camera_.viewH() * 0.5f};
    }
    screen_ = Screen::Play;
    return true;
}

void App::updateJoin() {
    net_.pump();
    if (input_.cancel()) {
        screen_ = Screen::Multi;
        return;
    }
    if (!scanResults_.empty()) {
        if (input_.up()) {
            scanIndex_ = (scanIndex_ + static_cast<int>(scanResults_.size()) - 1) % static_cast<int>(scanResults_.size());
        }
        if (input_.downNav()) {
            scanIndex_ = (scanIndex_ + 1) % static_cast<int>(scanResults_.size());
        }
        if (input_.confirm()) {
            partyName_ = scanResults_[static_cast<size_t>(scanIndex_)];
            if (net_.join(partyName_)) {
                status_ = "Joined " + partyName_;
            } else {
                status_ = net_.lastError();
            }
        }
    }
    uint8_t myId = 0, team = 0, mode = 0, diff = 1;
    uint16_t mapIndex = 0;
    if (net_.takeWelcome(myId, team, mapIndex, mode, diff)) {
        teamPick_ = team;
        modePick_ = std::min(kModeCount - 1, static_cast<int>(mode));
        difficultyPick_ = std::min(2, static_cast<int>(diff));
        if (static_cast<GameMode>(modePick_) == GameMode::Zombie) {
            teamPick_ = 1;
        }
        net_.sendTeamPick(static_cast<uint8_t>(teamPick_));
        if (mapIndex < maps_.size()) {
            soloMap_ = mapIndex;
        }
    }
    if (net_.connected()) {
        teamCountPick_ = std::max(2, static_cast<int>(net_.teamCount()));
        if ((input_.left() || input_.right()) && modeUsesTeams(static_cast<GameMode>(modePick_))) {
            teamPick_ = (teamPick_ + (input_.right() ? 1 : teamCountPick_ - 1)) % std::max(2, teamCountPick_);
            if (static_cast<GameMode>(modePick_) == GameMode::Zombie) {
                teamPick_ = 1;
            }
            net_.sendTeamPick(static_cast<uint8_t>(teamPick_));
        }
        modePick_ = std::min(kModeCount - 1, static_cast<int>(net_.gameMode()));
        difficultyPick_ = std::min(2, static_cast<int>(net_.gameDifficulty()));
        if (net_.lobbyMap() < maps_.size()) {
            soloMap_ = net_.lobbyMap();
        }
    }
    uint16_t startMap = 0;
    uint8_t startMode = 0, startDiff = 1;
    if (net_.takeStart(startMap, startMode, startDiff)) {
        soloMap_ = startMap % static_cast<int>(std::max<size_t>(1, maps_.size()));
        modePick_ = std::min(kModeCount - 1, static_cast<int>(startMode));
        difficultyPick_ = std::min(2, static_cast<int>(startDiff));
        teamCountPick_ = std::max(2, static_cast<int>(net_.teamCount()));
        const auto& e = maps_[static_cast<size_t>(soloMap_)];
        world_.loadMap(joinPath(mapsDir_, e.file));
        Team teamJoin = teamFromIndex(teamPick_);
        if (static_cast<GameMode>(modePick_) == GameMode::Zombie) {
            teamJoin = Team::Counter;
        }
        world_.startMatch(teamJoin, 0, net_.localId(), static_cast<Difficulty>(difficultyPick_),
                          static_cast<GameMode>(modePick_), teamCountPick_);
        screen_ = Screen::Play;
    }
}

void App::updateEnter() {
    if (input_.cancel() && partyName_.empty()) {
        screen_ = Screen::Multi;
        return;
    }
    if (input_.left()) {
        nameCursor_ = (nameCursor_ + 35) % 36;
    }
    if (input_.right()) {
        nameCursor_ = (nameCursor_ + 1) % 36;
    }
    if (input_.confirm()) {
        if (partyName_.size() < 8) {
            partyName_.push_back(kCharset[nameCursor_]);
        }
    }
    if (input_.cancel()) {
        if (!partyName_.empty()) {
            partyName_.pop_back();
        } else {
            screen_ = Screen::Multi;
        }
        return;
    }
    if (input_.start() && !partyName_.empty()) {
        if (net_.join(partyName_)) {
            screen_ = Screen::Join;
            status_ = "Connecting " + partyName_;
        } else {
            status_ = net_.lastError();
        }
    }
}

void App::updatePlay() {
    if (world_.matchOver()) {
        if (input_.cancel() || input_.start()) {
            net_.leave();
            menuIndex_ = 0;
            world_.setPaused(false);
            goTo(Screen::Menu);
        }
        return;
    }
    if (input_.start()) {
        world_.setPaused(!world_.paused());
    }
    if (world_.paused() && input_.cancel()) {
        net_.leave();
        menuIndex_ = 0;
        world_.setPaused(false);
        goTo(Screen::Menu);
    }
}

void App::updateEditorSize() {
    if (input_.cancel()) {
        editorMap_.clear();
        menuIndex_ = 2;
        goTo(Screen::Menu);
        return;
    }
    std::vector<int> saved;
    collectSaved(saved);
    const int total = 4 + static_cast<int>(saved.size());
    if (input_.up()) {
        editPick_ = (editPick_ + total - 1) % std::max(1, total);
    }
    if (input_.downNav()) {
        editPick_ = (editPick_ + 1) % std::max(1, total);
    }
    if (input_.confirm()) {
        if (editPick_ < 4) {
            beginNewEditor(editPick_);
        } else {
            const int si = editPick_ - 4;
            if (si >= 0 && si < static_cast<int>(saved.size())) {
                beginSavedEditor(saved[static_cast<size_t>(si)]);
            }
        }
    }
}

void App::beginNewEditor(int sizeIdx) {
    editSize_ = sizeIdx;
    editorMap_.createBlank(kEditorDims[editSize_], kEditorDims[editSize_], kEditorSizeIds[editSize_], "dust",
                           "NEW MAP");
    editX_ = editY_ = 2;
    editTile_ = 1;
    editZoom_ = zoomForSize(kEditorSizeIds[editSize_]);
    editFile_.clear();
    status_.clear();
    goTo(Screen::Editor);
}

void App::beginSavedEditor(int mapIndex) {
    if (mapIndex < 0 || mapIndex >= static_cast<int>(maps_.size())) {
        status_ = "No saved map";
        return;
    }
    const auto& e = maps_[static_cast<size_t>(mapIndex)];
    if (!editorMap_.load(joinPath(mapsDir_, e.file))) {
        status_ = "Load failed";
        return;
    }
    editFile_ = e.file;
    editX_ = editY_ = 2;
    editTile_ = 1;
    editZoom_ = zoomForSize(editorMap_.sizeName());
    status_.clear();
    goTo(Screen::Editor);
}

void App::collectSaved(std::vector<int>& out) const {
    out.clear();
    for (int i = 0; i < static_cast<int>(maps_.size()); ++i) {
        if (isSavedMap(maps_[static_cast<size_t>(i)])) {
            out.push_back(i);
        }
    }
}

int App::nextSavedNumber() const {
    int best = 0;
    for (const auto& e : maps_) {
        if (!isSavedMap(e)) {
            continue;
        }
        best = std::max(best, parseSavedNum(e.file));
        best = std::max(best, parseSavedNum(e.name));
    }
    DIR* d = opendir(mapsDir_.c_str());
    if (d) {
        while (dirent* ent = readdir(d)) {
            const std::string f = ent->d_name;
            if (f.find("saved_") != std::string::npos) {
                best = std::max(best, parseSavedNum(f));
            }
        }
        closedir(d);
    }
    return best + 1;
}

void App::ensureMapsDir() {
#ifdef PSP
    sceIoMkdir(mapsDir_.c_str(), 0777);
#else
    mkdir(mapsDir_.c_str(), 0755);
#endif
}

void App::refreshSavedMaps() {
    importSavedMaps();
    orderMaps();
}

void App::importSavedMaps() {
    DIR* d = opendir(mapsDir_.c_str());
    if (!d) {
        return;
    }
    while (dirent* ent = readdir(d)) {
        const std::string f = ent->d_name;
        const bool savedName = f.rfind("saved_", 0) == 0 || f.rfind("SAVED_", 0) == 0;
        if (!savedName) {
            continue;
        }
        if (f.size() < 4 || f.compare(f.size() - 4, 4, ".csp") != 0) {
            continue;
        }
        bool found = false;
        for (auto& e : maps_) {
            if (e.file == f) {
                found = true;
                GameMap tmp;
                if (tmp.load(joinPath(mapsDir_, f))) {
                    e.size = tmp.sizeName();
                    e.theme = tmp.theme();
                    if (!tmp.name().empty()) {
                        e.name = tmp.name();
                    }
                }
                break;
            }
        }
        if (found) {
            continue;
        }
        GameMap tmp;
        if (!tmp.load(joinPath(mapsDir_, f))) {
            continue;
        }
        std::string nm = tmp.name();
        if (nm.empty() || nm == "NEW MAP") {
            nm = f.substr(0, f.size() - 4);
            for (char& c : nm) {
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
            }
        }
        maps_.push_back({f, tmp.sizeName().empty() ? "small" : tmp.sizeName(), tmp.theme(), nm});
    }
    closedir(d);
}

void App::orderMaps() {
    std::vector<MapEntry> saved;
    std::vector<MapEntry> rest;
    saved.reserve(maps_.size());
    rest.reserve(maps_.size());
    for (auto& e : maps_) {
        if (isSavedMap(e)) {
            saved.push_back(e);
        } else {
            rest.push_back(e);
        }
    }
    std::sort(saved.begin(), saved.end(), [](const MapEntry& a, const MapEntry& b) {
        return parseSavedNum(a.file) < parseSavedNum(b.file);
    });
    maps_.swap(saved);
    maps_.insert(maps_.end(), rest.begin(), rest.end());
}

void App::updateEditor(float dt) {
    float zoomDir = 0.0f;
    if (input_.lHeld()) {
        zoomDir -= 1.0f;
    }
    if (input_.rHeld()) {
        zoomDir += 1.0f;
    }
    if (zoomDir != 0.0f) {
        editZoom_ *= std::exp(zoomDir * 2.8f * dt);
        const float minZ = editorMap_.width() >= 400 ? 12.0f : (editorMap_.width() >= 200 ? 9.0f : 6.0f);
        editZoom_ = clamp(editZoom_, minZ, 72.0f);
    }
    if (inputLock_ > 0.0f) {
        return;
    }

    if (input_.select() || input_.cancel()) {
        editorMap_.clear();
        status_.clear();
        refreshSavedMaps();
        goTo(Screen::EditorSize);
        return;
    }
    if (input_.start()) {
        ensureMapsDir();
        if (editFile_.empty()) {
            const int n = nextSavedNumber();
            char fname[32];
            char dname[32];
            std::snprintf(fname, sizeof(fname), "saved_%02d.csp", n);
            std::snprintf(dname, sizeof(dname), "SAVED_%02d", n);
            editFile_ = fname;
            editorMap_.setName(dname);
        }
        const std::string path = joinPath(mapsDir_, editFile_);
        if (editorMap_.save(path)) {
            MapEntry entry{editFile_, editorMap_.sizeName(), editorMap_.theme(), editorMap_.name()};
            bool found = false;
            for (auto& e : maps_) {
                if (e.file == editFile_) {
                    e = entry;
                    found = true;
                    break;
                }
            }
            if (!found) {
                maps_.insert(maps_.begin(), entry);
            }
            GameMap::appendIndex(mapsDir_, entry);
            refreshSavedMaps();
            for (int i = 0; i < static_cast<int>(maps_.size()); ++i) {
                if (maps_[static_cast<size_t>(i)].file == editFile_) {
                    soloMap_ = i;
                    break;
                }
            }
            status_ = std::string("Saved ") + editorMap_.name();
            assets_.resetDrawState(renderer_);
        } else {
            status_ = "Save failed";
        }
        return;
    }

    const bool pal = input_.down(SDL_CONTROLLER_BUTTON_X);
    const int palCols = 6;
    if (pal) {
        if (input_.left()) {
            editTile_ = (editTile_ + kEditTileCount - 1) % kEditTileCount;
        }
        if (input_.right()) {
            editTile_ = (editTile_ + 1) % kEditTileCount;
        }
        if (input_.up()) {
            editTile_ = (editTile_ + kEditTileCount - palCols) % kEditTileCount;
        }
        if (input_.downNav()) {
            editTile_ = (editTile_ + palCols) % kEditTileCount;
        }
    } else {
        int dx = 0, dy = 0;
        if (input_.up()) {
            dy = -1;
        }
        if (input_.downNav()) {
            dy = 1;
        }
        if (input_.left()) {
            dx = -1;
        }
        if (input_.right()) {
            dx = 1;
        }
        const float ax = input_.analogX();
        const float ay = input_.analogY();
        const Uint32 now = SDL_GetTicks();
        const float mag = std::sqrt(ax * ax + ay * ay);
        if (mag > 0.28f && now - editRepeatMs_ > (mag > 0.75f ? 28 : 70)) {
            editRepeatMs_ = now;
            if (ax > 0.28f) {
                dx = 1;
            } else if (ax < -0.28f) {
                dx = -1;
            }
            if (ay > 0.28f) {
                dy = 1;
            } else if (ay < -0.28f) {
                dy = -1;
            }
            if (mag > 0.85f) {
                dx *= 2;
                dy *= 2;
            }
        }
        editX_ = std::max(0, std::min(editorMap_.width() - 1, editX_ + dx));
        editY_ = std::max(0, std::min(editorMap_.height() - 1, editY_ + dy));
        if (input_.down(SDL_CONTROLLER_BUTTON_A)) {
            editorMap_.set(editX_, editY_, kEditTileIds[editTile_]);
        }
    }
}

void App::render() {
    assets_.resetDrawState(renderer_);
    SDL_SetRenderDrawColor(renderer_, 12, 16, 14, 255);
    SDL_RenderClear(renderer_);
    switch (screen_) {
        case Screen::Menu:
            renderMenu();
            break;
        case Screen::Solo:
            renderSolo();
            break;
        case Screen::Mode:
            renderMode();
            break;
        case Screen::Difficulty:
            renderDifficulty();
            break;
        case Screen::Skin:
            renderSkin();
            break;
        case Screen::Multi:
            renderMulti();
            break;
        case Screen::Host:
            renderLobby(true);
            break;
        case Screen::Join:
            renderLobby(false);
            break;
        case Screen::Enter:
            renderEnter();
            break;
        case Screen::Play:
            renderPlay();
            break;
        case Screen::EditorSize:
            renderEditorSize();
            break;
        case Screen::Editor:
            renderEditor();
            break;
    }
    SDL_RenderPresent(renderer_);
}

void App::renderPanel() {
    assets_.draw(renderer_, "cs_dust_floor", 0, 0, 0, 15.0f);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 8, 12, 10, 200);
    SDL_Rect overlay{0, 0, kScreenW, kScreenH};
    SDL_RenderFillRect(renderer_, &overlay);
    box(renderer_, 24, 18, kScreenW - 48, kScreenH - 36, {18, 28, 22, 230}, {90, 160, 80, 255});
}

void App::renderMenu() {
    renderPanel();
    assets_.drawText(renderer_, "COUNTER-STRIKE 2D", 70, 36, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "PSP HOMEBREW", 160, 58, {140, 180, 120, 255}, false);
    assets_.drawCentered(renderer_, "soldier1_gun", 90, 150, -30, 1.6f);
    assets_.drawCentered(renderer_, "manBrown_machine", 390, 150, 210, 1.6f);
    for (int i = 0; i < 4; ++i) {
        const SDL_Color c = i == menuIndex_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        const std::string line = std::string(i == menuIndex_ ? "> " : "  ") + kMenu[i];
        assets_.drawText(renderer_, line, 180, 92 + i * 22, c, false);
    }
    assets_.drawText(renderer_, "Analog move+aim   R fire   Cross ok", 70, 230, {140, 150, 130, 255}, false);
}

void App::renderSolo() {
    renderPanel();
    assets_.drawText(renderer_, "SOLO MATCH", 150, 28, {230, 220, 160, 255}, true);
    char info[96];
    const int players = maps_.empty() ? 0 : playerCountForSize(maps_[static_cast<size_t>(soloMap_)].size);
    std::snprintf(info, sizeof(info), "Team %s   Size %s   %d players",
                  maps_.empty() ? "T"
                                : teamTag(teamPick_, maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size),
                                          GameMode::Normal),
                  soloFilter_ == 0 ? "all" : sizeLabel(kSizes[soloFilter_]), players);
    assets_.drawText(renderer_, info, 40, 52, {180, 200, 160, 255}, false);
    int drawn = 0;
    for (int i = 0; i < static_cast<int>(maps_.size()) && drawn < 8; ++i) {
        const int idx = (soloMap_ + i) % static_cast<int>(maps_.size());
        const auto& e = maps_[static_cast<size_t>(idx)];
        if (soloFilter_ != 0 && e.size != kSizes[soloFilter_]) {
            continue;
        }
        const SDL_Color c = drawn == 0 ? SDL_Color{255, 220, 80, 255} : SDL_Color{180, 180, 170, 255};
        char line[128];
        std::snprintf(line, sizeof(line), "%s  [%s]  %dp", e.name.c_str(), sizeLabel(e.size), playerCountForSize(e.size));
        assets_.drawText(renderer_, std::string(drawn == 0 ? "> " : "  ") + line, 40, 74 + drawn * 16, c, false);
        ++drawn;
    }
    assets_.drawText(renderer_, "L/R size  Left/Right team  Cross next  Circle back", 28, 230,
                     {140, 150, 130, 255}, false);
}

void App::renderMode() {
    renderPanel();
    assets_.drawText(renderer_, hostModeSelect_ ? "HOST MODE" : "GAME MODE", 150, 28, {230, 220, 160, 255}, true);
    for (int i = 0; i < kModeCount; ++i) {
        const GameMode m = static_cast<GameMode>(i);
        const bool sel = i == modePick_;
        const SDL_Color c = sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        assets_.drawText(renderer_, std::string(sel ? "> " : "  ") + gameModeName(m), 70, 58 + i * 22, c, true);
    }
    const GameMode m = static_cast<GameMode>(modePick_);
    assets_.drawText(renderer_, gameModeHint(m), 50, 172, {160, 180, 150, 255}, false);
    if (modeUsesTeams(m) && !maps_.empty()) {
        char teams[48];
        std::snprintf(teams, sizeof(teams), "Teams %d / %d", teamCountPick_,
                      maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size));
        assets_.drawText(renderer_, teams, 50, 188, {200, 210, 170, 255}, false);
    }
    if (m == GameMode::Zombie) {
        assets_.drawCentered(renderer_, "zoimbie1_hold", 390, 120, 20, 1.8f);
    } else if (m == GameMode::SoloVsAll) {
        assets_.drawCentered(renderer_, "soldier1_machine", 390, 120, -20, 1.6f);
    } else if (m == GameMode::LastSurvivor) {
        assets_.drawCentered(renderer_, "manBrown_gun", 390, 120, 200, 1.6f);
    } else if (m == GameMode::ProtectBase) {
        assets_.drawCentered(renderer_, "robot1_machine", 390, 120, 210, 1.6f);
    } else {
        assets_.drawCentered(renderer_, "robot1_machine", 390, 120, 210, 1.6f);
    }
    const char* modeHelp = hostModeSelect_
                               ? (modeUsesTeams(m) ? "Up/Down mode  Left/Right teams  Cross lobby"
                                                   : "Up/Down mode  Cross lobby")
                               : (modeUsesTeams(m) ? "Up/Down mode  Left/Right teams  Cross next"
                                                   : "Up/Down mode  Cross next");
    assets_.drawText(renderer_, modeHelp, 20, 230, {140, 150, 130, 255}, false);
}

void App::renderDifficulty() {
    renderPanel();
    assets_.drawText(renderer_, "DIFFICULTY", 150, 28, {230, 220, 160, 255}, true);
    const GameMode mode = static_cast<GameMode>(modePick_);
    if (!maps_.empty()) {
        const auto& e = maps_[static_cast<size_t>(soloMap_)];
        char sub[128];
        if (modeUsesTeams(mode)) {
            std::snprintf(sub, sizeof(sub), "%s   %s   Team %s   %d teams   %d players", e.name.c_str(),
                          gameModeName(mode), teamTag(teamPick_, teamCountPick_, mode), teamCountPick_,
                          playerCountForSize(e.size));
        } else {
            std::snprintf(sub, sizeof(sub), "%s   %s   %d players", e.name.c_str(), gameModeName(mode),
                          playerCountForSize(e.size));
        }
        assets_.drawText(renderer_, sub, 40, 52, {180, 200, 160, 255}, false);
    }
    static const char* kNames[] = {"EASY", "MEDIUM", "HARD"};
    static const char* kHints[] = {"Slow bots, low damage, pistols / SMG", "Rifles, standard aim and HP",
                                   "Fast bots, AWP mix, heavy damage"};
    for (int i = 0; i < 3; ++i) {
        const bool sel = i == difficultyPick_;
        const SDL_Color c = sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        assets_.drawText(renderer_, std::string(sel ? "> " : "  ") + kNames[i], 160, 88 + i * 28, c, true);
        if (sel) {
            assets_.drawText(renderer_, kHints[i], 50, 178, {160, 180, 150, 255}, false);
        }
    }
    assets_.drawText(renderer_,
                     modePicksTeamSkin(mode) ? "Up/Down select  Cross next  Circle back"
                                            : "Up/Down select  Cross start  Circle back",
                     40, 230, {140, 150, 130, 255}, false);
}

void App::renderSkin() {
    renderPanel();
    assets_.drawText(renderer_, "TEAM SKIN", 165, 28, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "Allies share this look. Enemy teams get other skins.", 36, 50,
                     {160, 180, 150, 255}, false);
    for (int i = 0; i < kSkinCount; ++i) {
        const int col = i % 4;
        const int row = i / 4;
        const float x = 78.0f + static_cast<float>(col) * 96.0f;
        const float y = 108.0f + static_cast<float>(row) * 78.0f;
        if (i == skinPick_) {
            box(renderer_, static_cast<int>(x) - 28, static_cast<int>(y) - 30, 56, 58, {40, 50, 28, 220},
                {255, 220, 80, 255});
        }
        drawCspspSkin(renderer_, assets_, i, x, y, 1.15f, -1.15f);
    }
    assets_.drawText(renderer_, "D-Pad choose  Cross start  Circle back", 70, 230, {140, 150, 130, 255}, false);
}

void App::renderMulti() {
    renderPanel();
    assets_.drawText(renderer_, "MULTIPLAYER", 140, 36, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "Host on one PSP, up to 16 players", 80, 58, {160, 180, 150, 255}, false);
    for (int i = 0; i < 4; ++i) {
        const SDL_Color c = i == menuIndex_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        assets_.drawText(renderer_, std::string(i == menuIndex_ ? "> " : "  ") + kMulti[i], 160, 90 + i * 22, c, false);
    }
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 40, 220, {255, 180, 80, 255}, false);
    }
}

void App::renderLobby(bool host) {
    renderPanel();
    assets_.drawText(renderer_, host ? "HOST LOBBY" : "JOIN PARTY", 140, 28, {230, 220, 160, 255}, true);
    char line[96];
    std::snprintf(line, sizeof(line), "Party %s   Players %d/16", net_.partyName().c_str(), net_.peerCount());
    assets_.drawText(renderer_, line, 40, 50, {180, 200, 160, 255}, false);
    const int modeId = host ? modePick_ : static_cast<int>(net_.gameMode());
    const GameMode mode = static_cast<GameMode>(modeId);
    const int mapIdx = host ? soloMap_ : static_cast<int>(net_.lobbyMap());
    std::string mapName = "---";
    if (!maps_.empty() && mapIdx >= 0 && mapIdx < static_cast<int>(maps_.size())) {
        mapName = maps_[static_cast<size_t>(mapIdx)].name;
    }
    assets_.drawText(renderer_, "Map " + mapName, 40, 66, {200, 200, 180, 255}, false);
    assets_.drawText(renderer_, std::string("Mode ") + gameModeName(mode), 40, 82, {255, 220, 80, 255}, false);
    if (modeUsesTeams(mode)) {
        const int nTeams = host ? teamCountPick_ : std::max(2, static_cast<int>(net_.teamCount()));
        char teamLine[48];
        std::snprintf(teamLine, sizeof(teamLine), "Team %s   %d teams", teamTag(teamPick_, nTeams, mode), nTeams);
        assets_.drawText(renderer_, teamLine, 240, 82, {200, 210, 180, 255}, false);
    }
    assets_.drawText(renderer_, gameModeHint(mode), 40, 98, {160, 180, 150, 255}, false);
    for (int i = 0; i < kMaxPlayers; ++i) {
        const NetPeer& p = net_.peers()[i];
        if (!p.connected && !(i == 0 && host)) {
            continue;
        }
        const int col = i / 6;
        const int row = i % 6;
        char n[32];
        const int nTeams = host ? teamCountPick_ : std::max(2, static_cast<int>(net_.teamCount()));
        const uint8_t team = (i == 0 && host) ? static_cast<uint8_t>(teamPick_) : p.team;
        const char* tag = modeUsesTeams(mode) ? teamTag(team, nTeams, mode) : "--";
        std::snprintf(n, sizeof(n), "%02d %s %s", i, p.connected ? p.name : (i == 0 ? "HOST" : ""), tag);
        assets_.drawText(renderer_, n, 40 + col * 200, 118 + row * 14, {200, 210, 190, 255}, false);
    }
    if (!scanResults_.empty() && !host && !net_.connected()) {
        for (int i = 0; i < static_cast<int>(scanResults_.size()) && i < 6; ++i) {
            const SDL_Color c = i == scanIndex_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{180, 180, 170, 255};
            assets_.drawText(renderer_, std::string(i == scanIndex_ ? "> " : "  ") + scanResults_[static_cast<size_t>(i)],
                             260, 118 + i * 14, c, false);
        }
    }
    const char* lobbyHelp = host
                                ? (modeUsesTeams(mode) ? "L/R mode  Select teams  Left/Right team  Up/Down map"
                                                       : "L/R mode  Up/Down map")
                                : (net_.connected()
                                       ? (modeUsesTeams(mode) ? "Left/Right team  Circle back" : "Circle back")
                                       : "Cross join   Circle back");
    assets_.drawText(renderer_, lobbyHelp, 18, 230, {140, 150, 130, 255}, false);
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 40, 214, {255, 180, 80, 255}, false);
    }
}

void App::renderEnter() {
    renderPanel();
    assets_.drawText(renderer_, "ENTER SERVER", 130, 36, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "Party name (max 8)", 40, 70, {180, 200, 160, 255}, false);
    box(renderer_, 40, 90, 200, 22, {10, 14, 12, 255}, {200, 200, 120, 255});
    assets_.drawText(renderer_, partyName_ + "_", 48, 94, {255, 255, 210, 255}, false);
    std::string row;
    for (int i = 0; i < 36; ++i) {
        if (i == nameCursor_) {
            row.push_back('[');
            row.push_back(kCharset[i]);
            row.push_back(']');
        } else {
            row.push_back(kCharset[i]);
        }
        if (i == 17) {
            assets_.drawText(renderer_, row, 40, 130, {220, 220, 200, 255}, false);
            row.clear();
        }
    }
    assets_.drawText(renderer_, row, 40, 148, {220, 220, 200, 255}, false);
    assets_.drawText(renderer_, "D-pad letter  Cross add  Circle del  Start connect", 24, 230,
                     {140, 150, 130, 255}, false);
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 40, 200, {255, 180, 80, 255}, false);
    }
}

void App::renderPlay() {
    world_.render(renderer_, assets_, camera_);
    world_.renderHud(renderer_, assets_);
    if (world_.matchOver()) {
        box(renderer_, 90, 80, 300, 110, {0, 0, 0, 210}, {220, 200, 80, 255});
        const bool win = world_.winnerId() == world_.localId();
        assets_.drawText(renderer_, win ? "YOU WIN" : "MATCH OVER", 160, 98, {255, 220, 80, 255}, true);
        assets_.drawText(renderer_, gameModeName(world_.mode()), 140, 124, {200, 210, 180, 255}, false);
        if (world_.mode() == GameMode::LastSurvivor) {
            char alive[32];
            std::snprintf(alive, sizeof(alive), "Last alive  id %d", world_.winnerId());
            assets_.drawText(renderer_, alive, 150, 144, {180, 200, 160, 255}, false);
        } else if (world_.mode() == GameMode::Zombie) {
            char w[32];
            std::snprintf(w, sizeof(w), "Wave %d", world_.wave());
            assets_.drawText(renderer_, w, 190, 144, {180, 200, 160, 255}, false);
        } else if (world_.mode() == GameMode::ProtectBase) {
            assets_.drawText(renderer_, win ? "Enemy base down" : "Your base fell", 150, 144,
                             {180, 200, 160, 255}, false);
        }
        assets_.drawText(renderer_, "Circle menu", 180, 164, {220, 220, 210, 255}, false);
    } else if (world_.paused()) {
        box(renderer_, 120, 90, 240, 90, {0, 0, 0, 200}, {220, 200, 80, 255});
        assets_.drawText(renderer_, "PAUSED", 190, 110, {255, 220, 80, 255}, true);
        assets_.drawText(renderer_, "Start resume   Circle menu", 140, 140, {220, 220, 210, 255}, false);
    }
}

void App::drawMapTile(int px, int py, int size, Tile t) {
    if (size < 1) {
        return;
    }
    const std::string floor = editorMap_.floorSprite();
    const std::string wall = editorMap_.wallSprite();
    assets_.drawFit(renderer_, t == Tile::Wall ? wall : floor, px, py, size, size);
    if (t == Tile::Crate) {
        assets_.drawFit(renderer_, editorMap_.csTile("crate"), px, py, size, size);
    } else if (t == Tile::Barrel) {
        assets_.drawFit(renderer_, editorMap_.csTile("barrel"), px, py, size, size);
    } else if (t == Tile::Cover) {
        assets_.drawFit(renderer_, editorMap_.csTile("cover"), px, py, size, size);
    } else if (t == Tile::Tree) {
        assets_.drawFit(renderer_, editorMap_.csTile("tree"), px, py, size, size);
    } else if (t == Tile::Door) {
        assets_.drawFit(renderer_, editorMap_.csTile("door"), px, py, size, size);
    } else if (t == Tile::SpawnT) {
        SDL_SetRenderDrawColor(renderer_, 255, 140, 40, 255);
        SDL_Rect m{px + size / 4, py + size / 4, size / 2, size / 2};
        SDL_RenderFillRect(renderer_, &m);
    } else if (t == Tile::SpawnCT) {
        SDL_SetRenderDrawColor(renderer_, 80, 140, 255, 255);
        SDL_Rect m{px + size / 4, py + size / 4, size / 2, size / 2};
        SDL_RenderFillRect(renderer_, &m);
    } else if (t == Tile::SiteA || t == Tile::SiteB) {
        SDL_SetRenderDrawColor(renderer_, 255, 220, 80, 255);
        SDL_Rect m{px + size / 3, py + size / 3, std::max(1, size / 3), std::max(1, size / 3)};
        SDL_RenderFillRect(renderer_, &m);
    }
}

void App::renderEditorSize() {
    renderPanel();
    assets_.drawText(renderer_, "MAP EDITOR", 150, 28, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "New map or edit a saved one", 90, 52, {160, 180, 150, 255}, false);
    std::vector<int> saved;
    collectSaved(saved);
    const int total = 4 + static_cast<int>(saved.size());
    const int shown = 7;
    int start = 0;
    if (editPick_ >= shown) {
        start = editPick_ - shown + 1;
    }
    for (int row = 0; row < shown && start + row < total; ++row) {
        const int i = start + row;
        const bool sel = i == editPick_;
        const SDL_Color c = sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        std::string label;
        if (i < 4) {
            label = kEditorSizeLabels[i];
        } else {
            const auto& e = maps_[static_cast<size_t>(saved[static_cast<size_t>(i - 4)])];
            label = e.name + "  [" + sizeLabel(e.size) + "]";
        }
        assets_.drawText(renderer_, std::string(sel ? "> " : "  ") + label, 70, 72 + row * 20, c, false);
    }
    assets_.drawText(renderer_, "Up/Down select  Cross open  Circle back", 40, 230, {140, 150, 130, 255}, false);
}

void App::renderEditor() {
    const float cell = editZoom_;
    const float viewW = static_cast<float>(kScreenW);
    const float viewH = static_cast<float>(kScreenH);
    float camX = editX_ * cell - viewW * 0.5f + cell * 0.5f;
    float camY = editY_ * cell - viewH * 0.5f + cell * 0.5f;
    const float maxX = std::max(0.0f, editorMap_.width() * cell - viewW);
    const float maxY = std::max(0.0f, editorMap_.height() * cell - viewH);
    camX = clamp(camX, 0.0f, maxX);
    camY = clamp(camY, 0.0f, maxY);

    const int tileSize = std::max(2, static_cast<int>(std::ceil(cell)) + 1);
    const int x0 = std::max(0, static_cast<int>(camX / cell) - 1);
    const int y0 = std::max(0, static_cast<int>(camY / cell) - 1);
    const int x1 = std::min(editorMap_.width(), static_cast<int>((camX + viewW) / cell) + 2);
    const int y1 = std::min(editorMap_.height(), static_cast<int>((camY + viewH) / cell) + 2);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const int px = static_cast<int>(x * cell - camX);
            const int py = static_cast<int>(y * cell - camY);
            drawMapTile(px, py, tileSize, editorMap_.at(x, y));
        }
    }
    SDL_Rect cur{static_cast<int>(editX_ * cell - camX), static_cast<int>(editY_ * cell - camY),
                 static_cast<int>(cell), static_cast<int>(cell)};
    SDL_SetRenderDrawColor(renderer_, 255, 230, 80, 255);
    SDL_RenderDrawRect(renderer_, &cur);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
    SDL_Rect bar{0, 0, kScreenW, 22};
    SDL_RenderFillRect(renderer_, &bar);
    const SDL_Color hudCol{240, 230, 180, 255};
    assets_.drawText(renderer_, "EDITOR", 6, 6, hudCol, false);
    assets_.drawText(renderer_, editorMap_.name(), 58, 6, hudCol, false);
    assets_.drawText(renderer_, "z", 150, 6, hudCol, false);
    assets_.drawInt(renderer_, static_cast<int>(editZoom_ + 0.5f), 160, 6, hudCol, false);
    assets_.drawInt(renderer_, editX_, 192, 6, hudCol, false);
    assets_.drawInt(renderer_, editY_, 224, 6, hudCol, false);
    assets_.drawText(renderer_, "Start save  Circle back", 268, 6, hudCol, false);
    const float zoomT = clamp((editZoom_ - 6.0f) / 66.0f, 0.0f, 1.0f);
    SDL_SetRenderDrawColor(renderer_, 60, 70, 50, 255);
    SDL_Rect zoomBg{150, 16, 66, 3};
    SDL_RenderFillRect(renderer_, &zoomBg);
    SDL_SetRenderDrawColor(renderer_, 255, 220, 80, 255);
    SDL_Rect zoomFg{150, 16, std::max(2, static_cast<int>(66.0f * zoomT)), 3};
    SDL_RenderFillRect(renderer_, &zoomFg);

    const int palCols = 6;
    const int palCell = 22;
    const int palPad = 6;
    const int palW = palCols * palCell + palPad * 2;
    const int palRows = (kEditTileCount + palCols - 1) / palCols;
    const int palH = palRows * palCell + palPad * 2 + 12;
    const int palX = 6;
    const int palY = kScreenH - palH - 6;
    box(renderer_, palX, palY, palW, palH, {12, 16, 14, 230}, {200, 200, 120, 255});
    assets_.drawText(renderer_, "TILES  hold Square", palX + 6, palY + 2, {230, 220, 160, 255}, false);
    for (int i = 0; i < kEditTileCount; ++i) {
        const int col = i % palCols;
        const int row = i / palCols;
        const int tx = palX + palPad + col * palCell;
        const int ty = palY + 14 + row * palCell;
        drawMapTile(tx + 1, ty + 1, palCell - 2, kEditTileIds[i]);
        if (i == editTile_) {
            SDL_SetRenderDrawColor(renderer_, 255, 230, 60, 255);
            SDL_Rect sel{tx, ty, palCell - 1, palCell - 1};
            SDL_RenderDrawRect(renderer_, &sel);
            SDL_Rect sel2{tx + 1, ty + 1, palCell - 3, palCell - 3};
            SDL_RenderDrawRect(renderer_, &sel2);
        }
    }

    assets_.drawText(renderer_, kEditTiles[editTile_], palX + palW + 8, palY + 8, {240, 230, 180, 255}, false);
    assets_.drawText(renderer_, "Hold L/R zoom  Cross paint", palX + palW + 8, palY + 24,
                     {160, 170, 150, 255}, false);
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, palX + palW + 8, palY + 40, {255, 200, 80, 255}, false);
    }
}

