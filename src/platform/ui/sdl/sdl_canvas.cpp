#include "sdl_canvas.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "resources/embedded_fonts.hpp"

namespace kr8::ui::sdl {
namespace {

struct Glyph {
    SDL_Rect source{};
    int advance = 0;
    int offset_x = 0;
    int offset_y = 0;
};

std::vector<std::uint32_t> utf8_codepoints(std::string_view text) {
    std::vector<std::uint32_t> result;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) {
            result.push_back(first);
            ++i;
            continue;
        }
        int extra = 0;
        std::uint32_t value = 0;
        if ((first & 0xE0) == 0xC0) {
            extra = 1;
            value = first & 0x1F;
        } else if ((first & 0xF0) == 0xE0) {
            extra = 2;
            value = first & 0x0F;
        } else if ((first & 0xF8) == 0xF0) {
            extra = 3;
            value = first & 0x07;
        } else {
            result.push_back('?');
            ++i;
            continue;
        }
        if (i + static_cast<std::size_t>(extra) >= text.size()) {
            result.push_back('?');
            break;
        }
        for (int j = 1; j <= extra; ++j) {
            value = (value << 6) |
                    (static_cast<unsigned char>(text[i + j]) & 0x3F);
        }
        result.push_back(value);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return result;
}

}  // namespace

struct SdlCanvas::Font {
    stbtt_fontinfo info{};
    std::vector<std::uint8_t> data;
    std::vector<Glyph> glyphs;
    SDL_Texture* atlas = nullptr;
    int ascent = 0;
    int line_height = 0;

    ~Font() {
        if (atlas != nullptr) {
            SDL_DestroyTexture(atlas);
        }
    }
};

namespace {

std::unique_ptr<SdlCanvas::Font> load_font(
    SDL_Renderer* renderer, std::span<const std::uint8_t> source,
    int pixel_height) {
    auto font = std::make_unique<SdlCanvas::Font>();
    font->data.assign(source.begin(), source.end());
    if (font->data.empty()) {
        throw std::runtime_error("embedded font is empty");
    }
    const int offset = stbtt_GetFontOffsetForIndex(font->data.data(), 0);
    if (offset < 0 ||
        stbtt_InitFont(&font->info, font->data.data(), offset) == 0) {
        throw std::runtime_error("cannot parse embedded font");
    }
    // Match the Win32 backend: GDI's negative font height is the em size, so
    // scale by em rather than by ascent-to-descent (which stb calls "pixel
    // height"). The latter produced text ~17-24% smaller on Linux.
    const float scale = stbtt_ScaleForMappingEmToPixels(
        &font->info, static_cast<float>(pixel_height));
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    font->ascent = static_cast<int>(ascent * scale);
    font->line_height = static_cast<int>((ascent - descent + line_gap) * scale);

    constexpr int kAtlasSize = 1024;
    std::vector<std::uint8_t> atlas(
        static_cast<std::size_t>(kAtlasSize) * kAtlasSize * 4, 0);
    int pen_x = 1;
    int pen_y = 1;
    int row_height = 0;

    for (int codepoint = 32; codepoint < 127; ++codepoint) {
        int advance = 0;
        int left_bearing = 0;
        stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance,
                                   &left_bearing);
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBox(&font->info, codepoint, scale, scale, &x0,
                                    &y0, &x1, &y1);
        const int width = std::max(0, x1 - x0);
        const int height = std::max(0, y1 - y0);
        if (pen_x + width + 1 >= kAtlasSize) {
            pen_x = 1;
            pen_y += row_height + 1;
            row_height = 0;
        }
        if (pen_y + height + 1 >= kAtlasSize) {
            throw std::runtime_error("font atlas is full");
        }
        if (width > 0 && height > 0) {
            std::vector<std::uint8_t> bitmap(
                static_cast<std::size_t>(width) * height);
            stbtt_MakeCodepointBitmap(&font->info, bitmap.data(), width,
                                      height, width, scale, scale, codepoint);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const std::size_t destination =
                        (static_cast<std::size_t>(pen_y + y) * kAtlasSize +
                         static_cast<std::size_t>(pen_x + x)) *
                        4;
                    atlas[destination + 0] = 255;
                    atlas[destination + 1] = 255;
                    atlas[destination + 2] = 255;
                    atlas[destination + 3] =
                        bitmap[static_cast<std::size_t>(y) * width + x];
                }
            }
        }

        Glyph glyph;
        glyph.source = SDL_Rect{pen_x, pen_y, width, height};
        glyph.advance = static_cast<int>(advance * scale);
        glyph.offset_x = x0;
        glyph.offset_y = y0;
        font->glyphs.push_back(glyph);
        pen_x += width + 1;
        row_height = std::max(row_height, height);
    }

    font->atlas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_STATIC, kAtlasSize,
                                    kAtlasSize);
    if (font->atlas == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_UpdateTexture(font->atlas, nullptr, atlas.data(), kAtlasSize * 4);
    SDL_SetTextureBlendMode(font->atlas, SDL_BLENDMODE_BLEND);
    return font;
}

}  // namespace

SdlCanvas::SdlCanvas(SDL_Renderer* renderer) : renderer_(renderer) {
    const EmbeddedFonts fonts = embedded_fonts();
    ui_ = load_font(renderer_, fonts.ui_regular, 14);
    ui_bold_ = load_font(renderer_, fonts.ui_bold, 14);
    mono_ = load_font(renderer_, fonts.mono_regular, 15);
    mono_small_ = load_font(renderer_, fonts.mono_regular, 13);
}

SdlCanvas::~SdlCanvas() = default;

SdlCanvas::Font& SdlCanvas::font(FontRole role) {
    switch (role) {
        case FontRole::Ui: return *ui_;
        case FontRole::UiBold: return *ui_bold_;
        case FontRole::Mono: return *mono_;
        case FontRole::MonoSmall: return *mono_small_;
    }
    return *ui_;
}

const SdlCanvas::Font& SdlCanvas::font(FontRole role) const {
    switch (role) {
        case FontRole::Ui: return *ui_;
        case FontRole::UiBold: return *ui_bold_;
        case FontRole::Mono: return *mono_;
        case FontRole::MonoSmall: return *mono_small_;
    }
    return *ui_;
}

void SdlCanvas::fill_rect(const Rect& rect, Color color) {
    if (rect.empty()) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    const SDL_FRect area{static_cast<float>(rect.left),
                         static_cast<float>(rect.top),
                         static_cast<float>(rect.width()),
                         static_cast<float>(rect.height())};
    SDL_RenderFillRect(renderer_, &area);
}

void SdlCanvas::frame_rect(const Rect& rect, Color color, int thickness) {
    if (rect.empty() || thickness <= 0) {
        return;
    }
    const int t = std::min(thickness, std::min(rect.width(), rect.height()));
    fill_rect(Rect{rect.left, rect.top, rect.right, rect.top + t}, color);
    fill_rect(Rect{rect.left, rect.bottom - t, rect.right, rect.bottom},
              color);
    fill_rect(Rect{rect.left, rect.top, rect.left + t, rect.bottom}, color);
    fill_rect(Rect{rect.right - t, rect.top, rect.right, rect.bottom}, color);
}

void SdlCanvas::text(int x, int y, std::string_view utf8, Color color,
                     FontRole role) {
    Font& selected = font(role);
    SDL_SetTextureColorMod(selected.atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(selected.atlas, color.a);
    int pen = x;
    for (const std::uint32_t codepoint : utf8_codepoints(utf8)) {
        const int index = codepoint >= 32 && codepoint < 127
                              ? static_cast<int>(codepoint) - 32
                              : '?' - 32;
        if (index < 0 ||
            index >= static_cast<int>(selected.glyphs.size())) {
            continue;
        }
        const Glyph& glyph = selected.glyphs[static_cast<std::size_t>(index)];
        if (glyph.source.w > 0 && glyph.source.h > 0) {
            const SDL_FRect source{
                static_cast<float>(glyph.source.x),
                static_cast<float>(glyph.source.y),
                static_cast<float>(glyph.source.w),
                static_cast<float>(glyph.source.h)};
            const SDL_FRect destination{
                static_cast<float>(pen + glyph.offset_x),
                static_cast<float>(y + selected.ascent + glyph.offset_y),
                static_cast<float>(glyph.source.w),
                static_cast<float>(glyph.source.h)};
            SDL_RenderTexture(renderer_, selected.atlas, &source,
                              &destination);
        }
        pen += glyph.advance;
    }
    SDL_SetTextureColorMod(selected.atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(selected.atlas, 255);
}

Size SdlCanvas::measure(std::string_view utf8, FontRole role) {
    const Font& selected = font(role);
    int width = 0;
    for (const std::uint32_t codepoint : utf8_codepoints(utf8)) {
        const int index = codepoint >= 32 && codepoint < 127
                              ? static_cast<int>(codepoint) - 32
                              : '?' - 32;
        if (index >= 0 &&
            index < static_cast<int>(selected.glyphs.size())) {
            width += selected.glyphs[static_cast<std::size_t>(index)].advance;
        }
    }
    return Size{width, selected.line_height};
}

int SdlCanvas::line_height(FontRole role) {
    return font(role).line_height;
}

void SdlCanvas::push_clip(const Rect& rect) {
    SDL_Rect current{};
    SDL_GetRenderClipRect(renderer_, &current);
    SDL_Rect next{rect.left, rect.top, rect.width(), rect.height()};
    if (current.w > 0 && current.h > 0) {
        const int left = std::max(current.x, next.x);
        const int top = std::max(current.y, next.y);
        const int right = std::min(current.x + current.w, next.x + next.w);
        const int bottom =
            std::min(current.y + current.h, next.y + next.h);
        next = SDL_Rect{left, top, std::max(0, right - left),
                        std::max(0, bottom - top)};
    }
    clip_stack_.push_back(current);
    SDL_SetRenderClipRect(renderer_, &next);
}

void SdlCanvas::pop_clip() {
    if (clip_stack_.empty()) {
        return;
    }
    const SDL_Rect previous = clip_stack_.back();
    clip_stack_.pop_back();
    SDL_SetRenderClipRect(renderer_,
                          previous.w > 0 && previous.h > 0 ? &previous
                                                           : nullptr);
}

}  // namespace kr8::ui::sdl
