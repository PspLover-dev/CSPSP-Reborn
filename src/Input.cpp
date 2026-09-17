#include "Input.hpp"

#include <cmath>
#include <cstring>

#ifdef PSP
#include <pspctrl.h>
#endif

void Input::init() {
#ifdef PSP
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#endif
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            pad_ = SDL_GameControllerOpen(i);
            break;
        }
    }
    keys_ = SDL_GetKeyboardState(nullptr);
}

void Input::beginFrame() {
    prevButtons_ = buttons_;
    if (keys_) {
        memcpy(prevKeys_, keys_, SDL_NUM_SCANCODES);
    }
}

void Input::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_QUIT) {
        quit_ = true;
    }
    if (e.type == SDL_CONTROLLERDEVICEADDED && !pad_) {
        pad_ = SDL_GameControllerOpen(e.cdevice.which);
    }
}

bool Input::down(int button) const { return (buttons_ & (1u << button)) != 0; }
bool Input::pressed(int button) const {
    const uint32_t bit = 1u << button;
    return (buttons_ & bit) && !(prevButtons_ & bit);
}
bool Input::released(int button) const {
    const uint32_t bit = 1u << button;
    return !(buttons_ & bit) && (prevButtons_ & bit);
}

bool Input::pressedKey(SDL_Scancode s) const { return keys_ && keys_[s] && !prevKeys_[s]; }
bool Input::downKey(SDL_Scancode s) const { return keys_ && keys_[s]; }

void Input::pollNative() {
    buttons_ = 0;
    analogX_ = 0.0f;
    analogY_ = 0.0f;

    if (pad_) {
        analogX_ = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
        analogY_ = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
        auto set = [&](SDL_GameControllerButton b) {
            if (SDL_GameControllerGetButton(pad_, b)) {
                buttons_ |= 1u << static_cast<int>(b);
            }
        };
        set(SDL_CONTROLLER_BUTTON_A);
        set(SDL_CONTROLLER_BUTTON_B);
        set(SDL_CONTROLLER_BUTTON_X);
        set(SDL_CONTROLLER_BUTTON_Y);
        set(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        set(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        set(SDL_CONTROLLER_BUTTON_START);
        set(SDL_CONTROLLER_BUTTON_BACK);
        set(SDL_CONTROLLER_BUTTON_DPAD_UP);
        set(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        set(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        set(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    }

#ifdef PSP
    SceCtrlData pad{};
    sceCtrlPeekBufferPositive(&pad, 1);
    const float nx = (static_cast<int>(pad.Lx) - 128) / 128.0f;
    const float ny = (static_cast<int>(pad.Ly) - 128) / 128.0f;
    if (std::fabs(nx) > std::fabs(analogX_)) {
        analogX_ = nx;
    }
    if (std::fabs(ny) > std::fabs(analogY_)) {
        analogY_ = ny;
    }
    if (pad.Buttons & PSP_CTRL_CROSS) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_A;
    }
    if (pad.Buttons & PSP_CTRL_CIRCLE) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_B;
    }
    if (pad.Buttons & PSP_CTRL_SQUARE) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_X;
    }
    if (pad.Buttons & PSP_CTRL_TRIANGLE) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_Y;
    }
    if (pad.Buttons & PSP_CTRL_LTRIGGER) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    }
    if (pad.Buttons & PSP_CTRL_RTRIGGER) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    }
    if (pad.Buttons & PSP_CTRL_START) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_START;
    }
    if (pad.Buttons & PSP_CTRL_SELECT) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_BACK;
    }
    if (pad.Buttons & PSP_CTRL_UP) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_DPAD_UP;
    }
    if (pad.Buttons & PSP_CTRL_DOWN) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    }
    if (pad.Buttons & PSP_CTRL_LEFT) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    }
    if (pad.Buttons & PSP_CTRL_RIGHT) {
        buttons_ |= 1u << SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    }
#endif

    analogX_ = clamp(analogX_, -1.0f, 1.0f);
    analogY_ = clamp(analogY_, -1.0f, 1.0f);
    if (std::fabs(analogX_) < kAnalogDeadzone) {
        analogX_ = 0.0f;
    }
    if (std::fabs(analogY_) < kAnalogDeadzone) {
        analogY_ = 0.0f;
    }

    // Keyboard analog fallback (desktop / PPSSPP keyboard mapping)
    if (keys_) {
        float kx = 0.0f, ky = 0.0f;
        if (keys_[SDL_SCANCODE_A] || keys_[SDL_SCANCODE_LEFT]) {
            kx -= 1.0f;
        }
        if (keys_[SDL_SCANCODE_D] || keys_[SDL_SCANCODE_RIGHT]) {
            kx += 1.0f;
        }
        if (keys_[SDL_SCANCODE_W] || keys_[SDL_SCANCODE_UP]) {
            ky -= 1.0f;
        }
        if (keys_[SDL_SCANCODE_S] || keys_[SDL_SCANCODE_DOWN]) {
            ky += 1.0f;
        }
        if (kx != 0.0f || ky != 0.0f) {
            analogX_ = kx;
            analogY_ = ky;
        }
    }
}
