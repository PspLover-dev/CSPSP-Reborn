#pragma once

#include "Types.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <string>
#include <unordered_map>
#include <vector>

struct Sprite {
    SDL_Texture* tex = nullptr;
    SDL_Rect src{0, 0, 0, 0};
};

class Assets {
public:
    bool load(SDL_Renderer* renderer, const std::string& root);
    void destroy();

    const Sprite* get(const std::string& name) const;
    SDL_Texture* texture(const std::string& file) const;

    void draw(SDL_Renderer* r, const std::string& name, float x, float y, float angleDeg = 0.0f,
              float scale = 1.0f, int alpha = 255) const;
    void drawFit(SDL_Renderer* r, const std::string& name, int x, int y, int w, int h, int alpha = 255) const;
    void drawFit(SDL_Renderer* r, const Sprite* s, int x, int y, int w, int h, int alpha = 255) const;
    void drawCentered(SDL_Renderer* r, const std::string& name, float cx, float cy, float angleDeg,
                      float scale = 1.0f) const;
    void drawFx(SDL_Renderer* r, const std::string& name, float cx, float cy, float angleDeg, float scale,
                int alpha, SDL_Color tint, bool additive = false) const;
    void drawHotspot(SDL_Renderer* r, const std::string& name, float x, float y, float angleDeg, float scale,
                     float hx, float hy, bool hflip = false) const;
    void drawHotspotXY(SDL_Renderer* r, const std::string& name, float x, float y, float angleDeg, float scaleX,
                       float scaleY, float hx, float hy, bool hflip = false) const;

    TTF_Font* fontSmall() const { return fontSmall_; }
    TTF_Font* fontBig() const { return fontBig_; }

    void drawText(SDL_Renderer* r, const std::string& text, int x, int y, SDL_Color color, bool big = false);
    void drawInt(SDL_Renderer* r, int value, int x, int y, SDL_Color color, bool big = false);
    int textWidth(const std::string& text, bool big = false) const;
    void resetDrawState(SDL_Renderer* r);

    std::string root() const { return root_; }

private:
    struct CachedText {
        std::string text;
        bool big = false;
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };
    struct Digit {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };

    void bakeDigits();
    const CachedText* cachedText(const std::string& text, bool big);
    bool ensureTexture(const std::string& file);
    void addSprite(const std::string& name, const std::string& file, int x, int y, int w, int h);
    void registerCspsp();

    std::string root_;
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures_;
    std::unordered_map<std::string, Sprite> sprites_;
    TTF_Font* fontSmall_ = nullptr;
    TTF_Font* fontBig_ = nullptr;
    std::vector<CachedText> textCache_;
    Digit digitsSmall_[11]{};
    Digit digitsBig_[11]{};
};
