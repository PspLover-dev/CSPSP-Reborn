#pragma once

#include "Types.hpp"

#include <SDL.h>

class Input {
public:
    void init();
    void beginFrame();
    void handleEvent(const SDL_Event& e);
    void pollNative();

    void setScheme(ControlScheme s) { scheme_ = s; }
    ControlScheme scheme() const { return scheme_; }

    float analogX() const { return analogX_; }
    float analogY() const { return analogY_; }
    Vec2 analog() const { return {analogX_, analogY_}; }
    float moveX() const;
    float moveY() const;
    float rotateAxis() const;

    bool down(int button) const;
    bool pressed(int button) const;
    bool released(int button) const;

    bool confirm() const { return pressed(SDL_CONTROLLER_BUTTON_A) || pressedKey(SDL_SCANCODE_RETURN); }
    bool cancel() const { return pressed(SDL_CONTROLLER_BUTTON_B) || pressedKey(SDL_SCANCODE_ESCAPE); }
    bool fireHeld() const;
    bool firePressed() const;
    bool reload() const;
    bool prevWeapon() const;
    bool nextWeapon() const;
    bool cycleWeapon() const { return nextWeapon(); }
    bool start() const { return pressed(SDL_CONTROLLER_BUTTON_START) || pressedKey(SDL_SCANCODE_F5); }
    bool select() const { return pressed(SDL_CONTROLLER_BUTTON_BACK); }
    bool up() const {
        return pressed(SDL_CONTROLLER_BUTTON_DPAD_UP) || pressedKey(SDL_SCANCODE_UP) || pressedKey(SDL_SCANCODE_W);
    }
    bool downNav() const {
        return pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || pressedKey(SDL_SCANCODE_DOWN) || pressedKey(SDL_SCANCODE_S);
    }
    bool left() const {
        return pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || pressedKey(SDL_SCANCODE_LEFT) || pressedKey(SDL_SCANCODE_A);
    }
    bool right() const {
        return pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || pressedKey(SDL_SCANCODE_RIGHT) ||
               pressedKey(SDL_SCANCODE_D);
    }
    bool lShoulder() const { return pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || pressedKey(SDL_SCANCODE_LEFTBRACKET); }
    bool rShoulder() const { return pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || pressedKey(SDL_SCANCODE_RIGHTBRACKET); }
    bool lHeld() const { return down(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || downKey(SDL_SCANCODE_LEFTBRACKET); }
    bool rHeld() const { return down(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || downKey(SDL_SCANCODE_RIGHTBRACKET); }

    bool quitRequested() const { return quit_; }

private:
    bool pressedKey(SDL_Scancode s) const;
    bool downKey(SDL_Scancode s) const;

    ControlScheme scheme_ = ControlScheme::Analog;
    SDL_GameController* pad_ = nullptr;
    float analogX_ = 0.0f;
    float analogY_ = 0.0f;
    uint32_t buttons_ = 0;
    uint32_t prevButtons_ = 0;
    const Uint8* keys_ = nullptr;
    Uint8 prevKeys_[SDL_NUM_SCANCODES]{};
    bool quit_ = false;
};
