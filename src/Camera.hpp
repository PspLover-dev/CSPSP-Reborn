#pragma once

#include "Types.hpp"

#include <SDL.h>
#include <cstdlib>

class Camera {
public:
    Vec2 pos;
    float zoom = 0.88f;

    float viewW() const { return kScreenW / zoom; }
    float viewH() const { return kScreenH / zoom; }

    void follow(const Vec2& target, float dt, float mapW, float mapH) {
        const float lerpT = clamp(dt * 8.0f, 0.0f, 1.0f);
        pos.x = lerp(pos.x, target.x - viewW() * 0.5f, lerpT);
        pos.y = lerp(pos.y, target.y - viewH() * 0.5f, lerpT);
        const float maxX = std::max(0.0f, mapW - viewW());
        const float maxY = std::max(0.0f, mapH - viewH());
        pos.x = clamp(pos.x, 0.0f, maxX);
        pos.y = clamp(pos.y, 0.0f, maxY);
    }

    void shake(float magnitude, float duration) {
        mag_ = magnitude;
        time_ = duration;
        dur_ = duration;
    }

    void update(float dt) {
        if (time_ > 0.0f) {
            time_ -= dt;
            const float t = dur_ > 0.0f ? time_ / dur_ : 0.0f;
            const float m = mag_ * t;
            offset_.x = ((std::rand() % 200) / 100.0f - 1.0f) * m;
            offset_.y = ((std::rand() % 200) / 100.0f - 1.0f) * m;
        } else {
            offset_ = {0.0f, 0.0f};
        }
    }

    Vec2 view() const { return pos + offset_; }

    Vec2 toScreen(float x, float y) const {
        const Vec2 v = view();
        return {(x - v.x) * zoom, (y - v.y) * zoom};
    }

    SDL_Rect worldToScreen(float x, float y, float w, float h) const {
        const Vec2 p = toScreen(x, y);
        return {static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(w * zoom), static_cast<int>(h * zoom)};
    }

    bool onScreen(float x, float y, float w, float h) const {
        const Vec2 v = view();
        return x + w >= v.x && y + h >= v.y && x <= v.x + viewW() && y <= v.y + viewH();
    }

private:
    Vec2 offset_;
    float mag_ = 0.0f;
    float time_ = 0.0f;
    float dur_ = 0.0f;
};
