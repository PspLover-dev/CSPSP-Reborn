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
const char* kMenu[] = {"CAREER", "NEW GAME", "MULTIPLAYER", "MAP EDITOR", "CONTROLS", "QUIT"};
constexpr int kMenuCount = 6;
const char* kCareerHub[] = {"SOLO", "SHOP", "STATS", "SAVE", "MAIN MENU"};
constexpr int kHubCount = 5;
const char* kMulti[] = {"HOST PARTY", "JOIN SCAN", "ENTER SERVER", "BACK"};
const char* kSizes[] = {"all", "small", "medium", "big", "extra"};
constexpr int kSizeFilterCount = 5;
const char* kEditTiles[] = {". floor", "# wall", "C crate", "B barrel", "D door", "~ water", "S cover", "R tree",
                            "N nuclear", "T spawn-T", "O spawn-CT", "A site-A", "X site-B"};
const Tile kEditTileIds[] = {Tile::Floor,  Tile::Wall,    Tile::Crate,   Tile::Barrel, Tile::Door, Tile::Water,
                             Tile::Cover,  Tile::Tree,    Tile::Nuclear, Tile::SpawnT, Tile::SpawnCT, Tile::SiteA,
                             Tile::SiteB};
constexpr int kEditTileCount = 13;
constexpr int kModeCount = static_cast<int>(GameMode::Count);
const int kEditorDims[] = {50, 100, 200, 500};
const char* kEditorSizeIds[] = {"small", "medium", "big", "extra"};
const char* kEditorSizeLabels[] = {"NEW SMALL  50x50", "NEW MEDIUM  100x100", "NEW BIG  200x200", "NEW LARGE  500x500"};
const char* kCharset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

bool isSavedMap(const MapEntry& e) {
    return e.file.find("saved_") != std::string::npos || e.name.find("SAVED_") != std::string::npos;
}

constexpr int kLoadoutCols = 5;
constexpr int kLoadoutRows = 3;

SDL_Color rarityTint(int r) {
    switch (r) {
        case 2:
            return {90, 210, 110, 255};
        case 3:
            return {80, 160, 255, 255};
        case 4:
            return {200, 110, 255, 255};
        case 5:
            return {255, 190, 60, 255};
        default:
            return {200, 200, 190, 255};
    }
}

int indexOfGun(const std::vector<WeaponId>& guns, WeaponId id) {
    for (int i = 0; i < static_cast<int>(guns.size()); ++i) {
        if (guns[static_cast<size_t>(i)] == id) {
            return i;
        }
    }
    return 0;
}

bool isOfficialMap(const MapEntry& e) {
    static const char* kOff[] = {"001_de_dust2_small.csp",
                                 "004_office2d_small.csp",
                                 "007_iceworld_small.csp",
                                 "010_fy_dodgeball_small.csp",
                                 "013_fy_nade_small.csp",
                                 "016_fy_poolday_small.csp",
                                 "019_castlevonbrown_small.csp",
                                 "022_circle_small.csp",
                                 "025_guano_small.csp",
                                 "028_silent_forest_small.csp",
                                 "031_small_forest_small.csp",
                                 "034_small_train_yard_small.csp",
                                 "037_lasertag_small.csp",
                                 "040_lasertag2_small.csp",
                                 "043_louismap_small.csp",
                                 "046_river_small.csp",
                                 "049_winter_small.csp"};
    for (const char* f : kOff) {
        if (e.file == f) {
            return true;
        }
    }
    return false;
}

std::string parentDir(const std::string& p) {
    const auto n = p.find_last_of("/\\");
    return n == std::string::npos ? std::string(".") : p.substr(0, n);
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
        return 10;
    }
    if (size == "medium") {
        return 18;
    }
    if (size == "big") {
        return 26;
    }
    return 34;
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
#ifdef PSP
    profilesDir_ = "ms0:/PSP/GAME/CSPSP/Profiles";
    sceIoMkdir("ms0:/PSP/GAME/CSPSP", 0777);
    sceIoMkdir(profilesDir_.c_str(), 0777);
#else
    profilesDir_ = joinPath(parentDir(gfxDir_), "Profiles");
#endif
    ensureMapsDir();
    profiles_.init(profilesDir_);
    if (!assets_.load(renderer_, gfxDir_)) {
        std::printf("Failed to load assets from %s\n", gfxDir_.c_str());
        return false;
    }
    input_.init();
    input_.setScheme(static_cast<ControlScheme>(profiles_.controlScheme));
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
        case Screen::CareerProfiles:
            updateCareerProfiles();
            break;
        case Screen::CareerName:
            updateCareerName();
            break;
        case Screen::CareerHub:
            updateCareerHub();
            break;
        case Screen::Shop:
            updateShop();
            break;
        case Screen::Stats:
            updateStats();
            break;
        case Screen::Controls:
            updateControls();
            break;
        case Screen::Solo:
            updateSolo();
            break;
        case Screen::Mode:
            updateMode();
            break;
        case Screen::Bases:
            updateBases();
            break;
        case Screen::Difficulty:
            updateDifficulty();
            break;
        case Screen::Skin:
            updateSkin();
            break;
        case Screen::Loadout:
            updateLoadout();
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
            if (screen_ == Screen::Play) {
                world_.update(dt, input_, camera_, net_.role() == NetRole::Offline ? nullptr : &net_);
                if (net_.role() != NetRole::Offline) {
                    net_.pump();
                }
            }
            break;
        case Screen::Results:
            updateResults(dt);
            break;
        case Screen::EditorSize:
            updateEditorSize();
            break;
        case Screen::EditorTheme:
            updateEditorTheme();
            break;
        case Screen::Editor:
            updateEditor(dt);
            break;
    }
}

void App::updateMenu() {
    if (input_.up()) {
        menuIndex_ = (menuIndex_ + kMenuCount - 1) % kMenuCount;
    }
    if (input_.downNav()) {
        menuIndex_ = (menuIndex_ + 1) % kMenuCount;
    }
    if (input_.confirm()) {
        if (menuIndex_ == 0) {
            menuIndex_ = 0;
            profiles_.refresh();
            goTo(Screen::CareerProfiles);
        } else if (menuIndex_ == 1) {
            careerFrom_ = false;
            menuIndex_ = 0;
            refreshSavedMaps();
            goTo(Screen::Solo);
        } else if (menuIndex_ == 2) {
            careerFrom_ = false;
            menuIndex_ = 0;
            goTo(Screen::Multi);
        } else if (menuIndex_ == 3) {
            editPick_ = 0;
            refreshSavedMaps();
            goTo(Screen::EditorSize);
        } else if (menuIndex_ == 4) {
            controlPick_ = static_cast<int>(profiles_.controlScheme);
            goTo(Screen::Controls);
        } else {
            running_ = false;
        }
    }
}

void App::updateSolo() {
    if (input_.cancel() || input_.start()) {
        menuIndex_ = 0;
        goTo(careerFrom_ ? Screen::CareerHub : Screen::Menu);
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
        if (!mapUnlocked(maps_[static_cast<size_t>(soloMap_)])) {
            status_ = "Map locked";
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
    if (!maps_.empty() && modeUsesTeams(static_cast<GameMode>(modePick_)) &&
        static_cast<GameMode>(modePick_) != GameMode::ProtectBase) {
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
        if (static_cast<GameMode>(modePick_) == GameMode::ProtectBase) {
            if (!maps_.empty()) {
                clampTeamPicks(teamPick_, teamCountPick_, maps_[static_cast<size_t>(soloMap_)].size,
                               GameMode::ProtectBase);
            }
            screen_ = Screen::Bases;
            return;
        }
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

void App::updateBases() {
    if (input_.cancel() || input_.start()) {
        screen_ = Screen::Mode;
        return;
    }
    int mx = 2;
    if (!maps_.empty()) {
        mx = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
    }
    clampTeamPicks(teamPick_, teamCountPick_, maps_.empty() ? "small" : maps_[static_cast<size_t>(soloMap_)].size,
                   GameMode::ProtectBase);
    if (input_.up()) {
        teamCountPick_ = teamCountPick_ <= 2 ? mx : teamCountPick_ - 1;
    }
    if (input_.downNav()) {
        teamCountPick_ = teamCountPick_ >= mx ? 2 : teamCountPick_ + 1;
    }
    clampTeamPicks(teamPick_, teamCountPick_, maps_.empty() ? "small" : maps_[static_cast<size_t>(soloMap_)].size,
                   GameMode::ProtectBase);
    if (!input_.confirm()) {
        return;
    }
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

void App::updateDifficulty() {
    if (input_.cancel() || input_.start()) {
        screen_ = static_cast<GameMode>(modePick_) == GameMode::ProtectBase ? Screen::Bases : Screen::Mode;
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
            prepareLoadout();
            goTo(Screen::Loadout);
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
        if (!skinAllowed(skinPick_)) {
            status_ = "Skin locked";
            return;
        }
        prepareLoadout();
        goTo(Screen::Loadout);
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
    if (careerFrom_) {
        if (CareerProfile* p = profiles_.current()) {
            p->matches++;
            p->loadout[0] = static_cast<int>(pendingLoadout_[0]);
            p->loadout[1] = static_cast<int>(pendingLoadout_[1]);
            p->loadout[2] = static_cast<int>(pendingLoadout_[2]);
        }
    }
    world_.startMatch(teamFromIndex(teamPick_), botCount_, 0, static_cast<Difficulty>(difficultyPick_), mode,
                      teamCountPick_, skin, makeOpts());
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
                      static_cast<GameMode>(modePick_), teamCountPick_, -1, makeOpts());
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
                          static_cast<GameMode>(modePick_), teamCountPick_, -1, makeOpts());
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
        beginResults(false);
        return;
    }
    if (input_.start()) {
        world_.setPaused(!world_.paused());
    }
    if (world_.paused() && input_.cancel()) {
        beginResults(true);
    }
}

void App::updateEditorSize() {
    if (input_.cancel()) {
        editorMap_.clear();
        menuIndex_ = 3;
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
    editTheme_ = 0;
    editX_ = editY_ = 2;
    editTile_ = 1;
    editZoom_ = zoomForSize(kEditorSizeIds[editSize_]);
    editFile_.clear();
    status_.clear();
    goTo(Screen::EditorTheme);
}

void App::updateEditorTheme() {
    if (input_.cancel()) {
        goTo(Screen::EditorSize);
        return;
    }
    if (input_.up()) {
        editTheme_ = (editTheme_ + GameMap::kThemeCount - 1) % GameMap::kThemeCount;
    }
    if (input_.downNav()) {
        editTheme_ = (editTheme_ + 1) % GameMap::kThemeCount;
    }
    if (input_.confirm()) {
        editorMap_.createBlank(kEditorDims[editSize_], kEditorDims[editSize_], kEditorSizeIds[editSize_],
                               GameMap::themeName(editTheme_), "NEW MAP");
        goTo(Screen::Editor);
    }
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
    editTheme_ = GameMap::themeIndex(editorMap_.theme());
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
        } else if (isOfficialMap(e)) {
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

    if (input_.cycleWeapon()) {
        const int next = (GameMap::themeIndex(editorMap_.theme()) + 1) % GameMap::kThemeCount;
        editorMap_.setTheme(GameMap::themeName(next));
        editTheme_ = next;
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
        case Screen::CareerProfiles:
            renderCareerProfiles();
            break;
        case Screen::CareerName:
            renderCareerName();
            break;
        case Screen::CareerHub:
            renderCareerHub();
            break;
        case Screen::Shop:
            renderShop();
            break;
        case Screen::Stats:
            renderStats();
            break;
        case Screen::Controls:
            renderControls();
            break;
        case Screen::Solo:
            renderSolo();
            break;
        case Screen::Mode:
            renderMode();
            break;
        case Screen::Bases:
            renderBases();
            break;
        case Screen::Difficulty:
            renderDifficulty();
            break;
        case Screen::Skin:
            renderSkin();
            break;
        case Screen::Loadout:
            renderLoadout();
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
        case Screen::Results:
            renderResults();
            break;
        case Screen::EditorSize:
            renderEditorSize();
            break;
        case Screen::EditorTheme:
            renderEditorTheme();
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
    for (int i = 0; i < kMenuCount; ++i) {
        const SDL_Color c = i == menuIndex_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        const std::string line = std::string(i == menuIndex_ ? "> " : "  ") + kMenu[i];
        assets_.drawText(renderer_, line, 180, 80 + i * 18, c, false);
    }
    assets_.drawText(renderer_, "Up/Down  Cross ok", 70, 230, {140, 150, 130, 255}, false);
}

void App::renderSolo() {
    renderPanel();
    assets_.drawText(renderer_, "CHOOSE MAP", 150, 28, {230, 220, 160, 255}, true);
    char info[96];
    const int players = maps_.empty() ? 0 : playerCountForSize(maps_[static_cast<size_t>(soloMap_)].size);
    std::snprintf(info, sizeof(info), "Team %s   Size %s   %d players",
                  maps_.empty() ? "T"
                                : teamTag(teamPick_, maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size),
                                          GameMode::Normal),
                  soloFilter_ == 0 ? "all" : sizeLabel(kSizes[soloFilter_]), players);
    assets_.drawText(renderer_, info, 40, 48, {180, 200, 160, 255}, false);
    int drawn = 0;
    for (int i = 0; i < static_cast<int>(maps_.size()) && drawn < 8; ++i) {
        const int idx = (soloMap_ + i) % static_cast<int>(std::max<size_t>(1, maps_.size()));
        const auto& e = maps_[static_cast<size_t>(idx)];
        if (soloFilter_ != 0 && e.size != kSizes[soloFilter_]) {
            continue;
        }
        const bool lock = !mapUnlocked(e);
        const SDL_Color c = drawn == 0 ? (lock ? SDL_Color{180, 90, 70, 255} : SDL_Color{255, 220, 80, 255})
                                       : (lock ? SDL_Color{110, 110, 100, 255} : SDL_Color{180, 180, 170, 255});
        char line[128];
        if (lock) {
            std::snprintf(line, sizeof(line), "%s  LV%d", e.name.c_str(), e.unlockLevel);
        } else {
            std::snprintf(line, sizeof(line), "%s  [%s]", e.name.c_str(), sizeLabel(e.size));
        }
        assets_.drawText(renderer_, std::string(drawn == 0 ? "> " : "  ") + line, 36, 68 + drawn * 14, c, false);
        ++drawn;
    }
    ensurePreview();
    drawPreview(300, 68, 150, 140);
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 36, 210, {220, 140, 90, 255}, false);
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
    if (modeUsesTeams(m) && m != GameMode::ProtectBase && !maps_.empty()) {
        char teams[48];
        std::snprintf(teams, sizeof(teams), "Teams %d / %d", teamCountPick_,
                      maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size));
        assets_.drawText(renderer_, teams, 50, 188, {200, 210, 170, 255}, false);
    } else if (m == GameMode::ProtectBase) {
        assets_.drawText(renderer_, "Next: choose how many bases", 50, 188, {200, 210, 170, 255}, false);
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
                               ? (modeUsesTeams(m) && m != GameMode::ProtectBase
                                      ? "Up/Down mode  Left/Right teams  Cross lobby"
                                      : "Up/Down mode  Cross next")
                               : (modeUsesTeams(m) && m != GameMode::ProtectBase
                                      ? "Up/Down mode  Left/Right teams  Cross next"
                                      : "Up/Down mode  Cross next");
    assets_.drawText(renderer_, modeHelp, 20, 230, {140, 150, 130, 255}, false);
}

void App::renderBases() {
    renderPanel();
    assets_.drawText(renderer_, "BASES", 190, 28, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "How many bases / teams on this map", 70, 52, {160, 180, 150, 255}, false);
    int mx = 2;
    if (!maps_.empty()) {
        mx = maxTeamsForSize(maps_[static_cast<size_t>(soloMap_)].size);
    }
    for (int n = 2; n <= mx; ++n) {
        const bool sel = n == teamCountPick_;
        const SDL_Color c = sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        char line[32];
        std::snprintf(line, sizeof(line), "%d BASES", n);
        assets_.drawText(renderer_, std::string(sel ? "> " : "  ") + line, 160, 80 + (n - 2) * 26, c, true);
    }
    assets_.drawText(renderer_, "Up/Down select  Cross next  Circle back", 40, 230, {140, 150, 130, 255}, false);
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
        if (!skinAllowed(i)) {
            assets_.drawText(renderer_, "LOCK", static_cast<int>(x) - 16, static_cast<int>(y) + 22,
                             {220, 80, 70, 255}, false);
        }
    }
    assets_.drawText(renderer_, "D-Pad choose  Cross next  Circle back", 70, 230, {140, 150, 130, 255}, false);
}

void App::renderMulti() {
    renderPanel();
    assets_.drawText(renderer_, "MULTIPLAYER", 140, 36, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "Host on one PSP, up to 34 players", 80, 58, {160, 180, 150, 255}, false);
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
        const bool board = world_.mode() == GameMode::Normal;
        box(renderer_, 90, 70, 300, board ? 168 : 110, {0, 0, 0, 210}, {220, 200, 80, 255});
        const bool win = world_.winnerId() == world_.localId();
        assets_.drawText(renderer_, win ? "YOU WIN" : "MATCH OVER", 160, 88, {255, 220, 80, 255}, true);
        assets_.drawText(renderer_, gameModeName(world_.mode()), 140, 114, {200, 210, 180, 255}, false);
        int footerY = 154;
        if (world_.mode() == GameMode::LastSurvivor) {
            char alive[32];
            std::snprintf(alive, sizeof(alive), "Last alive  id %d", world_.winnerId());
            assets_.drawText(renderer_, alive, 150, 134, {180, 200, 160, 255}, false);
        } else if (world_.mode() == GameMode::Zombie) {
            char w[32];
            std::snprintf(w, sizeof(w), "Wave %d", world_.wave());
            assets_.drawText(renderer_, w, 190, 134, {180, 200, 160, 255}, false);
        } else if (world_.mode() == GameMode::ProtectBase) {
            assets_.drawText(renderer_, win ? "Enemy base down" : "Your base fell", 150, 134,
                             {180, 200, 160, 255}, false);
        } else if (world_.mode() == GameMode::Normal) {
            int y = 134;
            int shown = 0;
            int ids[kMaxPlayers];
            int n = 0;
            for (int i = 0; i < kMaxPlayers; ++i) {
                const Actor* a = world_.actor(i);
                if (a && a->active) {
                    ids[n++] = i;
                }
            }
            std::sort(ids, ids + n, [&](int a, int b) {
                const Actor* aa = world_.actor(a);
                const Actor* bb = world_.actor(b);
                if (!aa || !bb) {
                    return a < b;
                }
                if (aa->kills != bb->kills) {
                    return aa->kills > bb->kills;
                }
                return aa->deaths < bb->deaths;
            });
            for (int i = 0; i < n && shown < 5; ++i) {
                const Actor* a = world_.actor(ids[i]);
                if (!a) {
                    continue;
                }
                char line[48];
                std::snprintf(line, sizeof(line), "%d  %s  %d", shown + 1, a->name.c_str(), a->kills);
                const SDL_Color col = a->id == world_.localId() ? SDL_Color{255, 220, 70, 255}
                                                                : SDL_Color{200, 210, 180, 255};
                assets_.drawText(renderer_, line, 140, y, col, false);
                y += 12;
                ++shown;
            }
            footerY = y + 6;
        }
        assets_.drawText(renderer_, "Circle menu", 180, footerY, {220, 220, 210, 255}, false);
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
    } else if (t == Tile::Water) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 40, 90, 180, 140);
        SDL_Rect wr{px, py, size, size};
        SDL_RenderFillRect(renderer_, &wr);
    } else if (t == Tile::Nuclear) {
        assets_.drawFit(renderer_, "cs_nuclear", px, py, size, size);
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

void App::renderEditorTheme() {
    renderPanel();
    assets_.drawText(renderer_, "MAP THEME", 155, 28, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "Tileset for this map", 130, 52, {160, 180, 150, 255}, false);
    const int shown = 8;
    int start = 0;
    if (editTheme_ >= shown) {
        start = editTheme_ - shown + 1;
    }
    for (int row = 0; row < shown && start + row < GameMap::kThemeCount; ++row) {
        const int i = start + row;
        const bool sel = i == editTheme_;
        const SDL_Color c = sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        assets_.drawText(renderer_, std::string(sel ? "> " : "  ") + GameMap::themeName(i), 160, 72 + row * 18, c,
                         false);
    }
    const std::string floor = std::string("cs_") + GameMap::themeName(editTheme_) + "_floor";
    const std::string wall = std::string("cs_") + GameMap::themeName(editTheme_) + "_wall";
    assets_.drawFit(renderer_, floor, 360, 90, 48, 48);
    assets_.drawFit(renderer_, wall, 360, 142, 48, 48);
    assets_.drawText(renderer_, "Up/Down select  Cross edit  Circle back", 36, 230, {140, 150, 130, 255}, false);
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
    assets_.drawText(renderer_, editorMap_.theme(), 150, 6, hudCol, false);
    assets_.drawText(renderer_, "z", 210, 6, hudCol, false);
    assets_.drawInt(renderer_, static_cast<int>(editZoom_ + 0.5f), 220, 6, hudCol, false);
    assets_.drawInt(renderer_, editX_, 252, 6, hudCol, false);
    assets_.drawInt(renderer_, editY_, 284, 6, hudCol, false);
    assets_.drawText(renderer_, "Start save", 330, 6, hudCol, false);
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
    assets_.drawText(renderer_, "Hold L/R zoom  Cross paint  Triangle theme", palX + palW + 8, palY + 24,
                     {160, 170, 150, 255}, false);
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, palX + palW + 8, palY + 40, {255, 200, 80, 255}, false);
    }
}

void App::updateCareerProfiles() {
    const int extra = 1;
    const int total = profiles_.count() + extra;
    if (input_.up()) {
        menuIndex_ = (menuIndex_ + total - 1) % std::max(1, total);
    }
    if (input_.downNav()) {
        menuIndex_ = (menuIndex_ + 1) % std::max(1, total);
    }
    if (input_.cancel()) {
        menuIndex_ = 0;
        goTo(Screen::Menu);
        return;
    }
    if (!input_.confirm()) {
        return;
    }
    if (menuIndex_ < profiles_.count()) {
        if (profiles_.loadIndex(menuIndex_)) {
            careerFrom_ = true;
            menuIndex_ = 0;
            goTo(Screen::CareerHub);
        }
    } else {
        newName_.clear();
        nameCursor_ = 0;
        goTo(Screen::CareerName);
    }
}

void App::updateCareerName() {
    if (input_.left()) {
        nameCursor_ = (nameCursor_ + 35) % 36;
    }
    if (input_.right()) {
        nameCursor_ = (nameCursor_ + 1) % 36;
    }
    if (input_.cancel()) {
        if (!newName_.empty()) {
            newName_.pop_back();
        } else {
            goTo(Screen::CareerProfiles);
        }
        return;
    }
    const bool confirmName = input_.start() || input_.select() ||
                             input_.pressed(SDL_CONTROLLER_BUTTON_Y) ||
                             (input_.confirm() && newName_.size() >= 8);
    if (confirmName) {
        if (profiles_.create(newName_)) {
            careerFrom_ = true;
            menuIndex_ = 0;
            status_.clear();
            goTo(Screen::CareerHub);
        } else {
            status_ = "Create failed";
        }
        return;
    }
    if (input_.confirm() && newName_.size() < 8) {
        newName_.push_back(kCharset[nameCursor_]);
    }
}

void App::updateCareerHub() {
    if (input_.up()) {
        menuIndex_ = (menuIndex_ + kHubCount - 1) % kHubCount;
    }
    if (input_.downNav()) {
        menuIndex_ = (menuIndex_ + 1) % kHubCount;
    }
    if (input_.cancel()) {
        menuIndex_ = 0;
        goTo(Screen::Menu);
        return;
    }
    if (!input_.confirm()) {
        return;
    }
    if (menuIndex_ == 0) {
        refreshSavedMaps();
        goTo(Screen::Solo);
    } else if (menuIndex_ == 1) {
        shopTab_ = 0;
        shopIndex_ = 0;
        goTo(Screen::Shop);
    } else if (menuIndex_ == 2) {
        goTo(Screen::Stats);
    } else if (menuIndex_ == 3) {
        status_ = profiles_.saveCurrent() ? "Saved" : "Save failed";
    } else {
        menuIndex_ = 0;
        goTo(Screen::Menu);
    }
}

void App::updateShop() {
    CareerProfile* p = profiles_.current();
    if (!p || input_.cancel()) {
        goTo(Screen::CareerHub);
        return;
    }
    if (input_.lShoulder()) {
        shopTab_ = (shopTab_ + 2) % 3;
        shopIndex_ = 0;
    }
    if (input_.rShoulder()) {
        shopTab_ = (shopTab_ + 1) % 3;
        shopIndex_ = 0;
    }
    std::vector<WeaponId> guns;
    collectShopGuns(guns);
    const int n = shopTab_ == 0 ? static_cast<int>(guns.size()) : (shopTab_ == 1 ? 3 : kSkinCount);
    if (n <= 0) {
        return;
    }
    if (shopTab_ == 2) {
        if (input_.up()) {
            shopIndex_ = (shopIndex_ + kSkinCount - 4) % kSkinCount;
        }
        if (input_.downNav()) {
            shopIndex_ = (shopIndex_ + 4) % kSkinCount;
        }
        if (input_.left()) {
            shopIndex_ = (shopIndex_ + kSkinCount - 1) % kSkinCount;
        }
        if (input_.right()) {
            shopIndex_ = (shopIndex_ + 1) % kSkinCount;
        }
    } else {
        if (input_.up()) {
            shopIndex_ = (shopIndex_ + n - 1) % n;
        }
        if (input_.downNav()) {
            shopIndex_ = (shopIndex_ + 1) % n;
        }
    }
    if (!input_.confirm()) {
        return;
    }
    if (shopTab_ == 0) {
        const WeaponId id = guns[static_cast<size_t>(shopIndex_)];
        if (p->hasGun(id)) {
            status_ = "Already owned";
            return;
        }
        const int price = weaponPrice(id);
        if (p->cash < price) {
            status_ = "Not enough cash";
            return;
        }
        p->cash -= price;
        p->guns[static_cast<int>(id)] = 1;
        profiles_.saveCurrent();
        status_ = "Unlocked";
    } else if (shopTab_ == 1) {
        const WeaponId id = grenadeFromIndex(shopIndex_);
        const int price = weaponPrice(id);
        if (p->nadeStock[shopIndex_] >= 9) {
            status_ = "Stock full";
            return;
        }
        if (p->cash < price) {
            status_ = "Not enough cash";
            return;
        }
        p->cash -= price;
        p->nadeStock[shopIndex_]++;
        profiles_.saveCurrent();
        status_ = "Stock +1";
    } else {
        if (p->skins[shopIndex_]) {
            status_ = "Already owned";
            return;
        }
        if (p->cash < 500) {
            status_ = "Not enough cash";
            return;
        }
        p->cash -= 500;
        p->skins[shopIndex_] = 1;
        profiles_.saveCurrent();
        status_ = "Skin unlocked";
    }
}

void App::updateStats() {
    if (input_.cancel() || input_.confirm()) {
        goTo(Screen::CareerHub);
    }
}

void App::updateControls() {
    if (input_.up() || input_.downNav()) {
        controlPick_ = 1 - controlPick_;
    }
    if (input_.confirm()) {
        profiles_.controlScheme = static_cast<uint8_t>(controlPick_);
        profiles_.persistSettings();
        input_.setScheme(static_cast<ControlScheme>(controlPick_));
        menuIndex_ = 4;
        goTo(Screen::Menu);
        return;
    }
    if (input_.cancel()) {
        goTo(Screen::Menu);
    }
}

void App::updateLoadout() {
    std::vector<WeaponId> guns;
    collectGuns(guns);
    if (guns.empty()) {
        guns.push_back(WeaponId::Glock);
    }
    const int n = static_cast<int>(guns.size());
    loadoutPick_ = std::max(0, std::min(loadoutPick_, n - 1));
    loadoutSlot_ = ((loadoutSlot_ % 3) + 3) % 3;
    if (input_.cancel()) {
        goTo(modePicksTeamSkin(static_cast<GameMode>(modePick_)) ? Screen::Skin : Screen::Difficulty);
        return;
    }
    if (input_.lShoulder()) {
        loadoutSlot_ = (loadoutSlot_ + 2) % 3;
        loadoutPick_ = indexOfGun(guns, pendingLoadout_[loadoutSlot_]);
    }
    if (input_.rShoulder()) {
        loadoutSlot_ = (loadoutSlot_ + 1) % 3;
        loadoutPick_ = indexOfGun(guns, pendingLoadout_[loadoutSlot_]);
    }
    if (input_.left()) {
        loadoutPick_ = (loadoutPick_ + n - 1) % n;
    }
    if (input_.right()) {
        loadoutPick_ = (loadoutPick_ + 1) % n;
    }
    if (input_.up()) {
        loadoutPick_ = (loadoutPick_ + n - kLoadoutCols) % n;
    }
    if (input_.downNav()) {
        loadoutPick_ = (loadoutPick_ + kLoadoutCols) % n;
    }
    if (input_.confirm()) {
        pendingLoadout_[loadoutSlot_] = guns[static_cast<size_t>(loadoutPick_)];
        loadoutSlot_ = (loadoutSlot_ + 1) % 3;
        loadoutPick_ = indexOfGun(guns, pendingLoadout_[loadoutSlot_]);
        return;
    }
    if (input_.start() || input_.select() || input_.pressed(SDL_CONTROLLER_BUTTON_Y)) {
        startSolo();
    }
}

void App::updateResults(float dt) {
    resultT_ += dt;
    const int cashTarget = resultCash_;
    const int xpTarget = resultXp_;
    resultCashShow_ = static_cast<int>(std::min(static_cast<float>(cashTarget), resultT_ * 180.0f));
    resultXpShow_ = static_cast<int>(std::min(static_cast<float>(xpTarget), resultT_ * 140.0f));
    if (resultT_ > 0.6f && (input_.confirm() || input_.cancel() || input_.start())) {
        net_.leave();
        world_.setPaused(false);
        menuIndex_ = 0;
        goTo(afterPlay());
    }
}

void App::renderCareerProfiles() {
    renderPanel();
    assets_.drawText(renderer_, "CAREER PROFILE", 120, 28, {230, 220, 160, 255}, true);
    const int n = profiles_.count();
    const int total = n + 1;
    constexpr int kVisible = 8;
    constexpr int kRowH = 16;
    const int listY = 54;
    int first = 0;
    if (menuIndex_ >= kVisible) {
        first = menuIndex_ - kVisible + 1;
    }
    if (first > std::max(0, total - kVisible)) {
        first = std::max(0, total - kVisible);
    }
    SDL_Rect clip{36, listY - 2, 408, kVisible * kRowH + 4};
    SDL_RenderSetClipRect(renderer_, &clip);
    for (int draw = 0; draw < kVisible && first + draw < total; ++draw) {
        const int i = first + draw;
        const bool sel = i == menuIndex_;
        const SDL_Color c = sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        std::string label = "NEW PROFILE";
        if (i < n) {
            label = profiles_.fileName(i);
            if (label.size() > 4) {
                const std::string ext = label.substr(label.size() - 4);
                if (ext == ".SAV" || ext == ".sav" || ext == ".bin" || ext == ".BIN") {
                    label = label.substr(0, label.size() - 4);
                }
            }
        }
        assets_.drawText(renderer_, std::string(sel ? "> " : "  ") + label, 48, listY + draw * kRowH, c, false);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    if (total > kVisible) {
        assets_.drawText(renderer_, first > 0 ? "^" : " ", 420, listY - 2, {160, 170, 150, 255}, false);
        assets_.drawText(renderer_, first + kVisible < total ? "v" : " ", 420, listY + kVisible * kRowH - 12,
                         {160, 170, 150, 255}, false);
    }
    assets_.drawText(renderer_, "Cross select  Circle back", 80, 230, {140, 150, 130, 255}, false);
}

void App::renderCareerName() {
    renderPanel();
    assets_.drawText(renderer_, "PROFILE NAME", 140, 28, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, newName_.empty() ? "_" : newName_, 160, 90, {255, 220, 80, 255}, true);
    drawCursorName(160, 130);
    assets_.drawText(renderer_, "Left/Right letter  Cross add  Triangle ok", 36, 230, {140, 150, 130, 255}, false);
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 40, 210, {255, 180, 80, 255}, false);
    }
}

void App::renderCareerHub() {
    renderPanel();
    assets_.drawText(renderer_, "CAREER", 36, 28, {230, 220, 160, 255}, true);
    if (const CareerProfile* p = profiles_.current()) {
        assets_.drawText(renderer_, p->name, 36, 50, {180, 200, 160, 255}, false);
    }
    drawCareerCorner();
    for (int i = 0; i < kHubCount; ++i) {
        const SDL_Color c = i == menuIndex_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        assets_.drawText(renderer_, std::string(i == menuIndex_ ? "> " : "  ") + kCareerHub[i], 170, 80 + i * 22, c,
                         false);
    }
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 50, 220, {255, 200, 80, 255}, false);
    }
}

void App::renderShop() {
    renderPanel();
    assets_.drawText(renderer_, "SHOP", 36, 24, {230, 220, 160, 255}, true);
    drawCareerCorner();
    const CareerProfile* p = profiles_.current();
    static const char* kTabs[] = {"WEAPONS", "GRENADES", "SKINS"};
    for (int i = 0; i < 3; ++i) {
        const SDL_Color c = i == shopTab_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{160, 160, 150, 255};
        assets_.drawText(renderer_, kTabs[i], 50 + i * 110, 48, c, false);
    }
    if (shopTab_ == 0) {
        std::vector<WeaponId> guns;
        collectShopGuns(guns);
        const int start = std::max(0, shopIndex_ - 4);
        int drawn = 0;
        for (int i = start; i < static_cast<int>(guns.size()) && drawn < 6; ++i) {
            const WeaponId id = guns[static_cast<size_t>(i)];
            const WeaponDef& w = weaponDef(id);
            const bool sel = i == shopIndex_;
            const bool own = p && p->hasGun(id);
            const int y = 68 + drawn * 24;
            if (sel) {
                box(renderer_, 32, y - 2, 416, 24, {40, 50, 28, 220}, {255, 220, 80, 255});
            }
            assets_.drawFit(renderer_, weaponGroundSprite(id), 38, y - 1, 22, 22);
            char line[96];
            std::snprintf(line, sizeof(line), "%s  %s  DMG %.0f  $%d%s", w.name, rarityLabel(w.rarity),
                          rarityDamage(id), weaponPrice(id), own ? "  OWN" : "");
            assets_.drawText(renderer_, line, 66, y + 4,
                             sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255}, false);
            ++drawn;
        }
    } else if (shopTab_ == 1) {
        for (int i = 0; i < 3; ++i) {
            const WeaponId id = grenadeFromIndex(i);
            const WeaponDef& w = weaponDef(id);
            const bool sel = i == shopIndex_;
            const int y = 78 + i * 36;
            if (sel) {
                box(renderer_, 40, y - 4, 400, 34, {40, 50, 28, 220}, {255, 220, 80, 255});
            }
            assets_.drawFit(renderer_, weaponGroundSprite(id), 50, y - 2, 28, 28);
            char line[80];
            std::snprintf(line, sizeof(line), "%s  stock %d  $%d", w.name, p ? p->nadeStock[i] : 0, weaponPrice(id));
            assets_.drawText(renderer_, line, 88, y + 6,
                             sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255}, false);
        }
    } else {
        for (int i = 0; i < kSkinCount; ++i) {
            const int col = i % 4;
            const int row = i / 4;
            const float x = 78.0f + static_cast<float>(col) * 96.0f;
            const float y = 110.0f + static_cast<float>(row) * 70.0f;
            if (i == shopIndex_) {
                box(renderer_, static_cast<int>(x) - 28, static_cast<int>(y) - 28, 56, 54, {40, 50, 28, 220},
                    {255, 220, 80, 255});
            }
            drawCspspSkin(renderer_, assets_, i, x, y, 1.05f, -1.15f);
            const bool own = p && p->skins[i];
            assets_.drawText(renderer_, own ? "OWN" : (i == 0 ? "FREE" : "$500"), static_cast<int>(x) - 16,
                             static_cast<int>(y) + 20, own ? SDL_Color{80, 220, 90, 255} : SDL_Color{220, 200, 80, 255},
                             false);
        }
    }
    if (!status_.empty()) {
        assets_.drawText(renderer_, status_, 36, 210, {255, 200, 80, 255}, false);
    }
    assets_.drawText(renderer_, "L/R tab  Up/Down select  Cross buy  Circle back", 28, 230,
                     {140, 150, 130, 255}, false);
}

void App::renderStats() {
    renderPanel();
    assets_.drawText(renderer_, "STATS", 36, 24, {230, 220, 160, 255}, true);
    drawCareerCorner();
    const CareerProfile* p = profiles_.current();
    if (!p) {
        return;
    }
    const float ratio = p->deaths > 0 ? static_cast<float>(p->kills) / static_cast<float>(p->deaths)
                                      : static_cast<float>(p->kills);
    char lines[10][64];
    std::snprintf(lines[0], 64, "Kills        %d", p->kills);
    std::snprintf(lines[1], 64, "Deaths       %d", p->deaths);
    std::snprintf(lines[2], 64, "Ratio        %.2f", ratio);
    std::snprintf(lines[3], 64, "Resist       %.0f%%", (1.0f - p->resistMul()) * 100.0f);
    std::snprintf(lines[4], 64, "Speed        %.0f%%", p->speedMul() * 100.0f);
    std::snprintf(lines[5], 64, "XP / Level   %d / %d", p->xp, p->level());
    std::snprintf(lines[6], 64, "Cash         $%d", p->cash);
    std::snprintf(lines[7], 64, "Matches      %d", p->matches);
    std::snprintf(lines[8], 64, "Bases down   %d", p->basesDestroyed);
    std::snprintf(lines[9], 64, "Max wave     %d", p->maxWave);
    for (int i = 0; i < 10; ++i) {
        assets_.drawText(renderer_, lines[i], 80, 54 + i * 16, {210, 210, 200, 255}, false);
    }
}

void App::renderControls() {
    renderPanel();
    assets_.drawText(renderer_, "CONTROLS", 170, 28, {230, 220, 160, 255}, true);
    const char* opts[] = {"ANALOG  move + aim", "D-PAD  move  L/R rotate"};
    for (int i = 0; i < 2; ++i) {
        const SDL_Color c = i == controlPick_ ? SDL_Color{255, 220, 80, 255} : SDL_Color{200, 200, 190, 255};
        assets_.drawText(renderer_, std::string(i == controlPick_ ? "> " : "  ") + opts[i], 70, 70 + i * 24, c, false);
    }
    assets_.drawText(renderer_, "Analog: stick move/aim  L prev  Y next  R/Cross fire", 30, 140,
                     {170, 180, 160, 255}, false);
    assets_.drawText(renderer_, "D-Pad: pad move  L/R turn  Square/Triangle guns  Cross fire", 30, 158,
                     {170, 180, 160, 255}, false);
    assets_.drawText(renderer_, "Cross ok  Circle back", 70, 230, {140, 150, 130, 255}, false);
}

void App::renderLoadout() {
    renderPanel();
    assets_.drawText(renderer_, "LOADOUT", 36, 22, {230, 220, 160, 255}, true);
    assets_.drawText(renderer_, "Pick 3 starting guns", 200, 24, {160, 170, 150, 255}, false);
    std::vector<WeaponId> guns;
    collectGuns(guns);
    if (guns.empty()) {
        guns.push_back(WeaponId::Glock);
    }
    const int n = static_cast<int>(guns.size());
    loadoutPick_ = std::max(0, std::min(loadoutPick_, n - 1));

    for (int i = 0; i < 3; ++i) {
        const int x = 32 + i * 148;
        const int y = 40;
        const bool sel = i == loadoutSlot_;
        box(renderer_, x, y, 140, 54, sel ? SDL_Color{48, 58, 28, 230} : SDL_Color{22, 30, 24, 230},
            sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{70, 90, 70, 255});
        const WeaponId id = pendingLoadout_[i];
        const WeaponDef& w = weaponDef(id);
        assets_.drawFit(renderer_, weaponGroundSprite(id), x + 8, y + 10, 52, 28);
        char slot[8];
        std::snprintf(slot, sizeof(slot), "%d", i + 1);
        assets_.drawText(renderer_, slot, x + 66, y + 6, sel ? SDL_Color{255, 220, 80, 255} : SDL_Color{160, 160, 150, 255},
                         false);
        assets_.drawText(renderer_, w.name, x + 80, y + 6, rarityTint(w.rarity), false);
        char stat[32];
        std::snprintf(stat, sizeof(stat), "DMG %.0f", rarityDamage(id));
        assets_.drawText(renderer_, stat, x + 66, y + 28, {190, 190, 180, 255}, false);
    }

    constexpr int cellW = 82;
    constexpr int cellH = 34;
    constexpr int gridX = 34;
    constexpr int gridY = 100;
    const int totalRows = (n + kLoadoutCols - 1) / kLoadoutCols;
    int firstRow = 0;
    const int pickRow = loadoutPick_ / kLoadoutCols;
    if (pickRow >= kLoadoutRows) {
        firstRow = pickRow - kLoadoutRows + 1;
    }
    if (firstRow > std::max(0, totalRows - kLoadoutRows)) {
        firstRow = std::max(0, totalRows - kLoadoutRows);
    }
    for (int r = 0; r < kLoadoutRows; ++r) {
        for (int c = 0; c < kLoadoutCols; ++c) {
            const int i = (firstRow + r) * kLoadoutCols + c;
            if (i >= n) {
                continue;
            }
            const int x = gridX + c * cellW;
            const int y = gridY + r * cellH;
            const WeaponId id = guns[static_cast<size_t>(i)];
            const bool pick = i == loadoutPick_;
            int used = -1;
            for (int s = 0; s < 3; ++s) {
                if (pendingLoadout_[s] == id) {
                    used = s;
                    break;
                }
            }
            SDL_Color fill = pick ? SDL_Color{50, 58, 30, 230} : SDL_Color{16, 22, 18, 210};
            SDL_Color edge = pick ? SDL_Color{255, 220, 80, 255}
                                  : (used >= 0 ? SDL_Color{180, 160, 70, 255} : SDL_Color{55, 70, 55, 255});
            box(renderer_, x, y, cellW - 4, cellH - 3, fill, edge);
            assets_.drawFit(renderer_, weaponGroundSprite(id), x + 6, y + 3, 52, 24);
            if (used >= 0) {
                char mark[4];
                std::snprintf(mark, sizeof(mark), "%d", used + 1);
                assets_.drawText(renderer_, mark, x + cellW - 16, y + 2, {255, 220, 80, 255}, false);
            }
        }
    }
    if (firstRow > 0) {
        assets_.drawText(renderer_, "^", 452, gridY - 2, {160, 170, 150, 255}, false);
    }
    if (firstRow + kLoadoutRows < totalRows) {
        assets_.drawText(renderer_, "v", 452, gridY + kLoadoutRows * cellH - 16, {160, 170, 150, 255}, false);
    }

    const WeaponId focus = guns[static_cast<size_t>(loadoutPick_)];
    const WeaponDef& fw = weaponDef(focus);
    char info[96];
    std::snprintf(info, sizeof(info), "%s   %s   DMG %.0f   MAG %d   RPM %.0f", fw.name, rarityLabel(fw.rarity),
                  rarityDamage(focus), fw.mag, fw.rpm);
    assets_.drawText(renderer_, info, 34, 206, rarityTint(fw.rarity), false);
    assets_.drawText(renderer_, "L/R slot  D-pad browse  Cross set  Triangle start", 28, 230,
                     {140, 150, 130, 255}, false);
}

void App::renderResults() {
    world_.render(renderer_, assets_, camera_);
    box(renderer_, 70, 50, 340, 172, {0, 0, 0, 220}, {220, 200, 80, 255});
    assets_.drawText(renderer_, resultQuit_ ? "MATCH LEFT" : (resultWon_ ? "YOU WIN" : "MATCH OVER"), 150, 64,
                     {255, 220, 80, 255}, true);
    assets_.drawText(renderer_, gameModeName(world_.mode()), 150, 90, {200, 210, 180, 255}, false);
    char k[48];
    std::snprintf(k, sizeof(k), "Kills %d   Wave %d", resultKills_, resultWave_);
    assets_.drawText(renderer_, k, 150, 110, {200, 200, 180, 255}, false);
    char cash[48];
    std::snprintf(cash, sizeof(cash), "CASH  +$%d", resultCashShow_);
    assets_.drawText(renderer_, cash, 150, 136, {80, 220, 90, 255}, true);
    char xp[48];
    std::snprintf(xp, sizeof(xp), "XP    +%d", resultXpShow_);
    assets_.drawText(renderer_, xp, 150, 164, {80, 180, 255, 255}, true);
    assets_.drawText(renderer_, "Cross continue", 170, 198, {160, 160, 150, 255}, false);
}

MatchOpts App::makeOpts() const {
    MatchOpts o;
    o.career = careerFrom_;
    o.loadout[0] = pendingLoadout_[0];
    o.loadout[1] = pendingLoadout_[1];
    o.loadout[2] = pendingLoadout_[2];
    if (careerFrom_) {
        if (const CareerProfile* p = profiles_.current()) {
            o.resistMul = p->resistMul();
            o.speedMul = p->speedMul();
            o.nadeStock[0] = p->nadeStock[0];
            o.nadeStock[1] = p->nadeStock[1];
            o.nadeStock[2] = p->nadeStock[2];
            o.careerNades = const_cast<int*>(p->nadeStock);
        }
    } else {
        o.resistMul = 1.0f;
        o.speedMul = 1.0f;
        o.nadeStock[0] = 2;
        o.nadeStock[1] = 2;
        o.nadeStock[2] = 1;
    }
    return o;
}

void App::beginResults(bool quit) {
    resultQuit_ = quit;
    resultWon_ = !quit && world_.localWon();
    resultKills_ = 0;
    resultWave_ = world_.wave();
    if (const Actor* me = world_.local()) {
        resultKills_ = me->kills;
        if (careerFrom_) {
            if (CareerProfile* p = profiles_.current()) {
                p->kills += me->kills;
                p->deaths += me->deaths;
            }
        }
    }
    if (careerFrom_) {
        if (CareerProfile* p = profiles_.current()) {
            p->basesDestroyed += world_.localBasesDestroyed();
            p->maxWave = std::max(p->maxWave, world_.wave());
        }
    }
    applyRewards(resultWon_, quit);
    resultCashShow_ = 0;
    resultXpShow_ = 0;
    resultT_ = 0.0f;
    world_.setPaused(true);
    net_.leave();
    goTo(Screen::Results);
}

void App::applyRewards(bool won, bool quit) {
    const float diff = difficultyPick_ == 2 ? 1.75f : (difficultyPick_ == 1 ? 1.35f : 1.0f);
    const float outcome = quit ? 0.45f : (won ? 1.40f : 0.55f);
    const GameMode mode = world_.mode();
    float cash = 60.0f;
    float xp = 35.0f;
    if (mode == GameMode::Zombie) {
        cash = 40.0f * static_cast<float>(std::max(1, world_.wave())) + 6.0f * static_cast<float>(resultKills_);
        xp = 25.0f * static_cast<float>(std::max(1, world_.wave())) + 4.0f * static_cast<float>(resultKills_);
    } else if (mode == GameMode::SoloVsAll) {
        cash = (won ? 220.0f : 70.0f) + 10.0f * static_cast<float>(resultKills_);
        xp = (won ? 120.0f : 40.0f) + 6.0f * static_cast<float>(resultKills_);
    } else if (mode == GameMode::LastSurvivor) {
        cash = (won ? 200.0f : 60.0f) + 8.0f * static_cast<float>(resultKills_);
        xp = (won ? 110.0f : 35.0f) + 5.0f * static_cast<float>(resultKills_);
    } else if (mode == GameMode::ProtectBase) {
        cash = (won ? 240.0f : 80.0f) + 20.0f * static_cast<float>(world_.localBasesDestroyed());
        xp = (won ? 130.0f : 45.0f) + 12.0f * static_cast<float>(world_.localBasesDestroyed());
    } else {
        cash = 80.0f + 8.0f * static_cast<float>(resultKills_);
        xp = 40.0f + 5.0f * static_cast<float>(resultKills_);
    }
    resultCash_ = std::max(5, static_cast<int>(cash * diff * outcome));
    resultXp_ = std::max(4, static_cast<int>(xp * diff * outcome));
    if (careerFrom_) {
        if (CareerProfile* p = profiles_.current()) {
            p->cash += resultCash_;
            p->xp += resultXp_;
            profiles_.saveCurrent();
        }
    }
}

void App::ensurePreview() {
    if (maps_.empty() || previewIdx_ == soloMap_) {
        return;
    }
    if (previewMap_.load(joinPath(mapsDir_, maps_[static_cast<size_t>(soloMap_)].file))) {
        previewIdx_ = soloMap_;
    }
}

void App::drawPreview(int px, int py, int maxW, int maxH) {
    if (previewIdx_ < 0 || previewMap_.width() < 1) {
        return;
    }
    const int tw = std::max(1, maxW / previewMap_.width());
    const int th = std::max(1, maxH / previewMap_.height());
    const int t = std::max(1, std::min(tw, th));
    const int w = previewMap_.width() * t;
    const int h = previewMap_.height() * t;
    box(renderer_, px - 2, py - 2, w + 4, h + 4, {10, 12, 10, 220}, {200, 200, 120, 255});
    const GameMap saved = editorMap_;
    editorMap_ = previewMap_;
    for (int y = 0; y < previewMap_.height(); ++y) {
        for (int x = 0; x < previewMap_.width(); ++x) {
            drawMapTile(px + x * t, py + y * t, t, previewMap_.at(x, y));
        }
    }
    editorMap_ = saved;
}

bool App::mapUnlocked(const MapEntry& e) const {
    if (isSavedMap(e)) {
        return true;
    }
    return playerLevel() >= std::max(0, e.unlockLevel);
}

int App::playerLevel() const {
    if (careerFrom_) {
        if (const CareerProfile* p = profiles_.current()) {
            return p->level();
        }
    }
    return std::max(1, profiles_.maxLevel());
}

bool App::gunAllowed(WeaponId id) const {
    if (id == WeaponId::Knife || id == WeaponId::Glock) {
        return true;
    }
    if (careerFrom_) {
        if (const CareerProfile* p = profiles_.current()) {
            return p->hasGun(id);
        }
        return false;
    }
    return profiles_.unionHasGun(id);
}

bool App::skinAllowed(int skin) const {
    if (skin == 0) {
        return true;
    }
    if (careerFrom_) {
        if (const CareerProfile* p = profiles_.current()) {
            return skin >= 0 && skin < 8 && p->skins[skin];
        }
        return false;
    }
    return profiles_.unionHasSkin(skin);
}

void App::drawCursorName(int x, int y) {
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
            assets_.drawText(renderer_, row, x, y, {220, 220, 200, 255}, false);
            row.clear();
        }
    }
    assets_.drawText(renderer_, row, x, y + 16, {220, 220, 200, 255}, false);
}

void App::prepareLoadout() {
    std::vector<WeaponId> guns;
    collectGuns(guns);
    if (careerFrom_) {
        if (const CareerProfile* p = profiles_.current()) {
            for (int i = 0; i < 3; ++i) {
                const auto id = static_cast<WeaponId>(p->loadout[i]);
                pendingLoadout_[i] = gunAllowed(id) ? id : guns[static_cast<size_t>(std::min(i, static_cast<int>(guns.size()) - 1))];
            }
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            pendingLoadout_[i] = guns[static_cast<size_t>(std::min(i, static_cast<int>(guns.size()) - 1))];
        }
    }
    loadoutSlot_ = 0;
    loadoutPick_ = indexOfGun(guns, pendingLoadout_[0]);
}

void App::collectShopGuns(std::vector<WeaponId>& out) const {
    out.clear();
    for (int i = 0; i < kWeaponCount; ++i) {
        const auto id = static_cast<WeaponId>(i);
        if (weaponIsLoadout(id) && !weaponDef(id).melee) {
            out.push_back(id);
        }
    }
}

void App::collectGuns(std::vector<WeaponId>& out) const {
    out.clear();
    for (int i = 0; i < kWeaponCount; ++i) {
        const auto id = static_cast<WeaponId>(i);
        if (!weaponIsLoadout(id) || weaponDef(id).melee) {
            continue;
        }
        if (gunAllowed(id)) {
            out.push_back(id);
        }
    }
    if (out.empty()) {
        out.push_back(WeaponId::Glock);
    }
}

void App::collectSkins(std::vector<int>& out) const {
    out.clear();
    for (int i = 0; i < kSkinCount; ++i) {
        if (skinAllowed(i)) {
            out.push_back(i);
        }
    }
}

void App::drawCareerCorner() {
    const CareerProfile* p = profiles_.current();
    if (!p) {
        return;
    }
    const int x = kScreenW - 122;
    const int y = 28;
    box(renderer_, x, y, 88, 40, {10, 16, 12, 230}, {90, 160, 80, 255});
    char cash[24];
    std::snprintf(cash, sizeof(cash), "$%d", p->cash);
    assets_.drawText(renderer_, cash, x + 6, y + 4, {80, 220, 90, 255}, false);
    char lv[32];
    std::snprintf(lv, sizeof(lv), "LVL%d", p->level());
    assets_.drawText(renderer_, lv, x + 6, y + 14, {255, 220, 80, 255}, false);
    SDL_Rect xpbg{x + 6, y + 26, 76, 6};
    SDL_SetRenderDrawColor(renderer_, 30, 30, 28, 255);
    SDL_RenderFillRect(renderer_, &xpbg);
    const float ratio = clamp(static_cast<float>(p->xpIntoLevel()) / static_cast<float>(std::max(1, p->xpToNext())), 0.0f, 1.0f);
    xpbg.w = std::max(1, static_cast<int>(76.0f * ratio));
    SDL_SetRenderDrawColor(renderer_, 80, 180, 255, 255);
    SDL_RenderFillRect(renderer_, &xpbg);
}

App::Screen App::afterPlay() const { return careerFrom_ ? Screen::CareerHub : Screen::Menu; }


