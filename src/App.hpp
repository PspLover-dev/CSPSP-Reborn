#pragma once

#include "Assets.hpp"
#include "Camera.hpp"
#include "Input.hpp"
#include "Map.hpp"
#include "Net.hpp"
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
        Solo,
        Mode,
        Bases,
        Difficulty,
        Skin,
        Multi,
        Host,
        Join,
        Enter,
        Play,
        EditorSize,
        EditorTheme,
        Editor
    };

    void update(float dt);
    void render();

    void updateMenu();
    void updateSolo();
    void updateMode();
    void updateBases();
    void updateDifficulty();
    void updateSkin();
    void updateMulti();
    void updateHost();
    void updateJoin();
    void updateEnter();
    void updatePlay();
    void updateEditorSize();
    void updateEditorTheme();
    void updateEditor(float dt);
    void goTo(Screen s);

    void renderMenu();
    void renderSolo();
    void renderMode();
    void renderBases();
    void renderDifficulty();
    void renderSkin();
    void renderMulti();
    void renderLobby(bool host);
    void renderEnter();
    void renderPlay();
    void renderEditorSize();
    void renderEditorTheme();
    void renderEditor();
    void renderPanel();
    void drawMapTile(int px, int py, int size, Tile t);

    bool startSolo();
    bool startHostedMatch();
    void drawCursorName(int x, int y);
    void beginNewEditor(int sizeIdx);
    void beginSavedEditor(int mapIndex);
    int nextSavedNumber() const;
    void importSavedMaps();
    void refreshSavedMaps();
    void orderMaps();
    void collectSaved(std::vector<int>& out) const;
    void ensureMapsDir();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Assets assets_;
    Input input_;
    Camera camera_;
    World world_;
    NetSession net_;
    Screen screen_ = Screen::Menu;
    bool running_ = true;

    std::vector<MapEntry> maps_;
    int menuIndex_ = 0;
    int soloMap_ = 0;
    int soloFilter_ = 0; // 0 all, 1 small, 2 medium, 3 big, 4 extra
    int teamPick_ = 1;   // A/T default can be 0..teamCount-1
    int teamCountPick_ = 2;
    int botCount_ = 7;
    int difficultyPick_ = 1; // 0 Easy, 1 Medium, 2 Hard
    int modePick_ = 0;
    int skinPick_ = 0;
    bool hostModeSelect_ = false;
    std::string partyName_ = "CSPSP01";
    int nameCursor_ = 0;
    std::vector<std::string> scanResults_;
    int scanIndex_ = 0;
    std::string status_;

    GameMap editorMap_;
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
};
