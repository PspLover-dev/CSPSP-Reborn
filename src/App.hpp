#pragma once

#include "Assets.hpp"
#include "Camera.hpp"
#include "Input.hpp"
#include "Map.hpp"
#include "Net.hpp"
#include "Profile.hpp"
#include "World.hpp"

#include <SDL.h>
#include <string>
#include <vector>

class App {
public:
    bool init();
    void run();
    void shutdown();

private:
    enum class Screen {
        Menu,
        CareerProfiles,
        CareerName,
        CareerHub,
        Shop,
        Stats,
        Controls,
        Solo,
        Mode,
        Bases,
        Difficulty,
        Skin,
        Loadout,
        Multi,
        Host,
        Join,
        Enter,
        Play,
        Results,
        EditorSize,
        EditorTheme,
        Editor
    };

    void update(float dt);
    void render();

    void updateMenu();
    void updateCareerProfiles();
    void updateCareerName();
    void updateCareerHub();
    void updateShop();
    void updateStats();
    void updateControls();
    void updateSolo();
    void updateMode();
    void updateBases();
    void updateDifficulty();
    void updateSkin();
    void updateLoadout();
    void updateMulti();
    void updateHost();
    void updateJoin();
    void updateEnter();
    void updatePlay();
    void updateResults(float dt);
    void updateEditorSize();
    void updateEditorTheme();
    void updateEditor(float dt);
    void goTo(Screen s);

    void renderMenu();
    void renderCareerProfiles();
    void renderCareerName();
    void renderCareerHub();
    void renderShop();
    void renderStats();
    void renderControls();
    void renderSolo();
    void renderMode();
    void renderBases();
    void renderDifficulty();
    void renderSkin();
    void renderLoadout();
    void renderMulti();
    void renderLobby(bool host);
    void renderEnter();
    void renderPlay();
    void renderResults();
    void renderEditorSize();
    void renderEditorTheme();
    void renderEditor();
    void renderPanel();
    void drawMapTile(int px, int py, int size, Tile t);
    void drawPreview(int px, int py, int maxW, int maxH);

    bool startSolo();
    bool startHostedMatch();
    MatchOpts makeOpts() const;
    void beginResults(bool quit);
    void applyRewards(bool won, bool quit);
    void drawCursorName(int x, int y);
    void beginNewEditor(int sizeIdx);
    void beginSavedEditor(int mapIndex);
    int nextSavedNumber() const;
    void importSavedMaps();
    void refreshSavedMaps();
    void orderMaps();
    void collectSaved(std::vector<int>& out) const;
    void ensureMapsDir();
    void ensurePreview();
    bool mapUnlocked(const MapEntry& e) const;
    int playerLevel() const;
    bool gunAllowed(WeaponId id) const;
    bool skinAllowed(int skin) const;
    void prepareLoadout();
    void collectGuns(std::vector<WeaponId>& out) const;
    void collectShopGuns(std::vector<WeaponId>& out) const;
    void collectSkins(std::vector<int>& out) const;
    void drawCareerCorner();
    Screen afterPlay() const;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Assets assets_;
    Input input_;
    Camera camera_;
    World world_;
    NetSession net_;
    ProfileStore profiles_;
    Screen screen_ = Screen::Menu;
    bool running_ = true;

    std::vector<MapEntry> maps_;
    int menuIndex_ = 0;
    int soloMap_ = 0;
    int soloFilter_ = 0;
    int teamPick_ = 1;
    int teamCountPick_ = 2;
    int botCount_ = 7;
    int difficultyPick_ = 1;
    int modePick_ = 0;
    int skinPick_ = 0;
    int loadoutPick_ = 0;
    int loadoutSlot_ = 0;
    int shopTab_ = 0;
    int shopIndex_ = 0;
    int controlPick_ = 0;
    bool careerFrom_ = false;
    bool hostModeSelect_ = false;
    WeaponId pendingLoadout_[3] = {WeaponId::Glock, WeaponId::USP, WeaponId::MP5};
    std::string partyName_ = "CSPSP01";
    int nameCursor_ = 0;
    std::string newName_;
    std::vector<std::string> scanResults_;
    int scanIndex_ = 0;
    std::string status_;

    GameMap editorMap_;
    GameMap previewMap_;
    int previewIdx_ = -1;
    int editX_ = 2, editY_ = 2;
    int editTile_ = 1;
    int editSize_ = 0;
    int editPick_ = 0;
    int editTheme_ = 0;
    std::string editFile_;
    float editZoom_ = 32.0f;
    float inputLock_ = 0.0f;
    Uint32 editRepeatMs_ = 0;
    std::string mapsDir_ = "maps";
    std::string gfxDir_ = "Gfx";
    std::string profilesDir_ = "Profiles";

    int resultCash_ = 0;
    int resultXp_ = 0;
    int resultCashShow_ = 0;
    int resultXpShow_ = 0;
    float resultT_ = 0.0f;
    bool resultWon_ = false;
    bool resultQuit_ = false;
    int resultKills_ = 0;
    int resultWave_ = 0;
};
