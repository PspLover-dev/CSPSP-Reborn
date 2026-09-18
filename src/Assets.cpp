#include "Assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

bool Assets::load(SDL_Renderer* renderer, const std::string& root) {
    renderer_ = renderer;
    root_ = root;

    const std::string atlasPath = joinPath(root, "atlas.txt");
    std::ifstream in(atlasPath);
    if (!in) {
        std::printf("Failed to open %s\n", atlasPath.c_str());
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string name, file;
        int x, y, w, h;
        if (!(ss >> name >> x >> y >> w >> h >> file)) {
            continue;
        }
        if (textures_.find(file) == textures_.end()) {
            const std::string path = joinPath(root, file);
            SDL_Texture* tex = IMG_LoadTexture(renderer, path.c_str());
            if (!tex) {
                std::printf("Missing texture %s (%s)\n", path.c_str(), IMG_GetError());
                continue;
            }
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            textures_[file] = tex;
        }
        Sprite spr;
        spr.tex = textures_[file];
        spr.src = {x, y, w, h};
        sprites_[name] = spr;
    }

    const std::string fontPath = joinPath(root, "font.ttf");
    fontSmall_ = TTF_OpenFont(fontPath.c_str(), 8);
    fontBig_ = TTF_OpenFont(fontPath.c_str(), 16);
    if (!fontSmall_ || !fontBig_) {
        std::printf("Font load failed: %s\n", TTF_GetError());
        return false;
    }
    registerCspsp();
    bakeDigits();
    return !sprites_.empty();
}

bool Assets::ensureTexture(const std::string& file) {
    if (textures_.find(file) != textures_.end()) {
        return true;
    }
    const std::string path = joinPath(root_, file);
    SDL_Surface* raw = IMG_Load(path.c_str());
    if (!raw) {
        std::printf("Missing texture %s (%s)\n", path.c_str(), IMG_GetError());
        return false;
    }
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    if (!conv) {
        std::printf("Convert failed %s\n", path.c_str());
        return false;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, conv);
    SDL_FreeSurface(conv);
    if (!tex) {
        std::printf("Texture failed %s (%s)\n", path.c_str(), SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    textures_[file] = tex;
    return true;
}

void Assets::addSprite(const std::string& name, const std::string& file, int x, int y, int w, int h) {
    auto it = textures_.find(file);
    if (it == textures_.end() || !it->second) {
        return;
    }
    Sprite spr;
    spr.tex = it->second;
    spr.src = {x, y, w, h};
    sprites_[name] = spr;
}

void Assets::registerCspsp() {
    // Exact quads from original GameStateLoading.cpp / players.png
    if (ensureTexture("players.png")) {
        for (int team = 0; team < 2; ++team) {
            for (int skin = 0; skin < 4; ++skin) {
                const int ox = (skin + team * 4) * 32;
                char base[24];
                std::snprintf(base, sizeof(base), "cs_p_%d%d", team, skin);
                addSprite(std::string(base) + "_body", "players.png", ox + 0, 0, 32, 16);
                addSprite(std::string(base) + "_head", "players.png", ox + 16, 16, 16, 16);
                addSprite(std::string(base) + "_arm", "players.png", ox + 8, 16, 8, 16);
                addSprite(std::string(base) + "_hand", "players.png", ox + 0, 16, 8, 16);
                addSprite(std::string(base) + "_legs", "players.png", ox + 0, 32, 32, 32);
            }
        }
    }
    if (ensureTexture("guns.png") && ensureTexture("gunsground.png")) {
        for (int id = 0; id < 32; ++id) {
            const int x = (id % 4) * 32;
            const int y = (id / 4) * 32;
            char hand[24], ground[24];
            std::snprintf(hand, sizeof(hand), "cs_gun_%d", id);
            std::snprintf(ground, sizeof(ground), "cs_ground_%d", id);
            addSprite(hand, "guns.png", x, y, 32, 32);
            addSprite(ground, "gunsground.png", x, y, 32, 32);
        }
    }
    if (ensureTexture("muzzleflash.png")) {
        for (int i = 0; i < 3; ++i) {
            char name[24];
            std::snprintf(name, sizeof(name), "cs_muzzle_%d", i);
            addSprite(name, "muzzleflash.png", i * 32, 0, 32, 32);
        }
    }
    // Original CSPSP particle sheet + scorch decal (GameStateLoading.cpp).
    if (ensureTexture("particles.png")) {
        addSprite("cs_explosion", "particles.png", 32, 0, 32, 32);
        addSprite("cs_flash", "particles.png", 64, 64, 32, 32);
        addSprite("cs_smoke", "particles.png", 0, 96, 32, 32);
    }
    if (ensureTexture("decals.png")) {
        addSprite("cs_scorch", "decals.png", 0, 0, 32, 32);
    }
    if (ensureTexture("cspsp_tiles.png")) {
        addSprite("cs_nuclear", "cspsp_tiles.png", 2 * 32, 3 * 32, 32, 32);
        static const char* kThemes[] = {"dust", "office", "inferno", "nuke", "vertigo",
                                        "cache", "aztec", "italy", "mill", "warehouse"};
        static const char* kSlots[] = {"floor", "wall", "alt", "crate", "barrel", "cover", "tree", "door"};
        for (int t = 0; t < 10; ++t) {
            for (int s = 0; s < 8; ++s) {
                addSprite(std::string("cs_") + kThemes[t] + "_" + kSlots[s], "cspsp_tiles.png", s * 32, t * 32, 32, 32);
            }
        }
    }
}

void Assets::destroy() {
    for (auto& kv : textures_) {
        SDL_DestroyTexture(kv.second);
    }
    textures_.clear();
    sprites_.clear();
    for (auto& c : textCache_) {
        if (c.tex) {
            SDL_DestroyTexture(c.tex);
        }
    }
    textCache_.clear();
    for (int i = 0; i < 11; ++i) {
        if (digitsSmall_[i].tex) {
            SDL_DestroyTexture(digitsSmall_[i].tex);
            digitsSmall_[i].tex = nullptr;
        }
        if (digitsBig_[i].tex) {
            SDL_DestroyTexture(digitsBig_[i].tex);
            digitsBig_[i].tex = nullptr;
        }
    }
    if (fontSmall_) {
        TTF_CloseFont(fontSmall_);
        fontSmall_ = nullptr;
    }
    if (fontBig_) {
        TTF_CloseFont(fontBig_);
        fontBig_ = nullptr;
    }
}

const Sprite* Assets::get(const std::string& name) const {
    auto it = sprites_.find(name);
    if (it == sprites_.end()) {
        return nullptr;
    }
    return &it->second;
}

SDL_Texture* Assets::texture(const std::string& file) const {
    auto it = textures_.find(file);
    return it == textures_.end() ? nullptr : it->second;
}

void Assets::draw(SDL_Renderer* r, const std::string& name, float x, float y, float angleDeg, float scale,
                  int alpha) const {
    const Sprite* s = get(name);
    if (!s || !s->tex) {
        return;
    }
    SDL_Rect dst{static_cast<int>(x), static_cast<int>(y), static_cast<int>(s->src.w * scale),
                 static_cast<int>(s->src.h * scale)};
    SDL_SetTextureAlphaMod(s->tex, static_cast<Uint8>(alpha));
    SDL_RenderCopyEx(r, s->tex, &s->src, &dst, angleDeg, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(s->tex, 255);
}

void Assets::drawFit(SDL_Renderer* r, const Sprite* s, int x, int y, int w, int h, int alpha) const {
    if (!s || !s->tex) {
        return;
    }
    SDL_Rect dst{x, y, w, h};
    if (alpha < 255) {
        SDL_SetTextureAlphaMod(s->tex, static_cast<Uint8>(alpha));
    }
    SDL_RenderCopy(r, s->tex, &s->src, &dst);
    if (alpha < 255) {
        SDL_SetTextureAlphaMod(s->tex, 255);
    }
}

void Assets::drawFit(SDL_Renderer* r, const std::string& name, int x, int y, int w, int h, int alpha) const {
    drawFit(r, get(name), x, y, w, h, alpha);
}

void Assets::drawCentered(SDL_Renderer* r, const std::string& name, float cx, float cy, float angleDeg,
                          float scale) const {
    drawFx(r, name, cx, cy, angleDeg, scale, 255, {255, 255, 255, 255}, false);
}

void Assets::drawFx(SDL_Renderer* r, const std::string& name, float cx, float cy, float angleDeg, float scale,
                    int alpha, SDL_Color tint, bool additive) const {
    const Sprite* s = get(name);
    if (!s || !s->tex || alpha <= 0 || scale <= 0.02f) {
        return;
    }
    const int dw = std::max(1, static_cast<int>(s->src.w * scale));
    const int dh = std::max(1, static_cast<int>(s->src.h * scale));
    SDL_Rect dst{static_cast<int>(cx) - dw / 2, static_cast<int>(cy) - dh / 2, dw, dh};
    SDL_Point center{dw / 2, dh / 2};
    SDL_SetTextureBlendMode(s->tex, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(s->tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(s->tex, static_cast<Uint8>(alpha > 255 ? 255 : alpha));
    SDL_RenderCopyEx(r, s->tex, &s->src, &dst, angleDeg, &center, SDL_FLIP_NONE);
    SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(s->tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(s->tex, 255);
}

void Assets::drawHotspot(SDL_Renderer* r, const std::string& name, float x, float y, float angleDeg, float scale,
                         float hx, float hy, bool hflip) const {
    drawHotspotXY(r, name, x, y, angleDeg, scale, scale, hx, hy, hflip);
}

void Assets::drawHotspotXY(SDL_Renderer* r, const std::string& name, float x, float y, float angleDeg, float scaleX,
                           float scaleY, float hx, float hy, bool hflip) const {
    const Sprite* s = get(name);
    if (!s || !s->tex) {
        return;
    }
    SDL_SetTextureColorMod(s->tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(s->tex, 255);

    // PSP SDL textures SDL_RenderGeometry as white quads. CopyEx is the working path.
    const int dw = std::max(1, static_cast<int>(std::lround(std::fabs(s->src.w * scaleX))));
    const int dh = std::max(1, static_cast<int>(std::lround(std::fabs(s->src.h * scaleY))));
    const float chx = hx * scaleX;
    const float chy = hy * scaleY;
    SDL_Rect dst{static_cast<int>(std::lround(x - chx)), static_cast<int>(std::lround(y - chy)), dw, dh};
    SDL_Point center{static_cast<int>(std::lround(chx)), static_cast<int>(std::lround(chy))};
    SDL_RenderCopyEx(r, s->tex, &s->src, &dst, angleDeg, &center, hflip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Assets::bakeDigits() {
    if (!renderer_ || !fontSmall_ || !fontBig_) {
        return;
    }
    const char* glyphs = "0123456789-";
    const SDL_Color white{255, 255, 255, 255};
    for (int i = 0; i < 11; ++i) {
        char s[2] = {glyphs[i], 0};
        auto bake = [&](TTF_Font* font, Digit& d) {
            if (d.tex) {
                SDL_DestroyTexture(d.tex);
                d.tex = nullptr;
            }
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, s, white);
            if (!surf) {
                return;
            }
            d.tex = SDL_CreateTextureFromSurface(renderer_, surf);
            d.w = surf->w;
            d.h = surf->h;
            SDL_FreeSurface(surf);
            if (d.tex) {
                SDL_SetTextureBlendMode(d.tex, SDL_BLENDMODE_BLEND);
            }
        };
        bake(fontSmall_, digitsSmall_[i]);
        bake(fontBig_, digitsBig_[i]);
    }
}

const Assets::CachedText* Assets::cachedText(const std::string& text, bool big) {
    for (const auto& c : textCache_) {
        if (c.big == big && c.text == text) {
            return &c;
        }
    }
    TTF_Font* font = big ? fontBig_ : fontSmall_;
    if (!font || !renderer_ || text.empty()) {
        return nullptr;
    }
    const SDL_Color white{255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), white);
    if (!surf) {
        return nullptr;
    }
    CachedText c;
    c.text = text;
    c.big = big;
    c.w = surf->w;
    c.h = surf->h;
    c.tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!c.tex) {
        return nullptr;
    }
    SDL_SetTextureBlendMode(c.tex, SDL_BLENDMODE_BLEND);
    if (textCache_.size() >= 128) {
        if (textCache_.front().tex) {
            SDL_DestroyTexture(textCache_.front().tex);
        }
        textCache_.erase(textCache_.begin());
    }
    textCache_.push_back(c);
    return &textCache_.back();
}

void Assets::drawText(SDL_Renderer* r, const std::string& text, int x, int y, SDL_Color color, bool big) {
    const CachedText* c = cachedText(text, big);
    if (!c || !c->tex) {
        return;
    }
    SDL_SetTextureColorMod(c->tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(c->tex, color.a);
    SDL_Rect dst{x, y, c->w, c->h};
    SDL_RenderCopy(r, c->tex, nullptr, &dst);
}

void Assets::drawInt(SDL_Renderer* r, int value, int x, int y, SDL_Color color, bool big) {
    Digit* digits = big ? digitsBig_ : digitsSmall_;
    if (value < 0) {
        Digit& minus = digits[10];
        if (minus.tex) {
            SDL_SetTextureColorMod(minus.tex, color.r, color.g, color.b);
            SDL_SetTextureAlphaMod(minus.tex, color.a);
            SDL_Rect dst{x, y, minus.w, minus.h};
            SDL_RenderCopy(r, minus.tex, nullptr, &dst);
            x += minus.w;
        }
        value = -value;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", value);
    for (const char* p = buf; *p; ++p) {
        const int idx = *p - '0';
        if (idx < 0 || idx > 9 || !digits[idx].tex) {
            continue;
        }
        Digit& d = digits[idx];
        SDL_SetTextureColorMod(d.tex, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(d.tex, color.a);
        SDL_Rect dst{x, y, d.w, d.h};
        SDL_RenderCopy(r, d.tex, nullptr, &dst);
        x += d.w;
    }
}

int Assets::textWidth(const std::string& text, bool big) const {
    TTF_Font* font = big ? fontBig_ : fontSmall_;
    if (!font) {
        return 0;
    }
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

void Assets::resetDrawState(SDL_Renderer* r) {
    if (r) {
        SDL_RenderSetClipRect(r, nullptr);
        SDL_RenderSetScale(r, 1.0f, 1.0f);
        SDL_RenderSetLogicalSize(r, kScreenW, kScreenH);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    }
    for (auto& kv : textures_) {
        if (!kv.second) {
            continue;
        }
        SDL_SetTextureAlphaMod(kv.second, 255);
        SDL_SetTextureColorMod(kv.second, 255, 255, 255);
        SDL_SetTextureBlendMode(kv.second, SDL_BLENDMODE_BLEND);
    }
}
