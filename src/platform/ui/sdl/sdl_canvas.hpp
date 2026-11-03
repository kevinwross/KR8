#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "kr8/ui/canvas.hpp"

namespace kr8::ui::sdl {

class SdlCanvas final : public Canvas {
public:
    explicit SdlCanvas(SDL_Renderer* renderer);
    ~SdlCanvas() override;

    SdlCanvas(const SdlCanvas&) = delete;
    SdlCanvas& operator=(const SdlCanvas&) = delete;

    void fill_rect(const Rect& rect, Color color) override;
    void frame_rect(const Rect& rect, Color color, int thickness) override;
    void text(int x, int y, std::string_view utf8, Color color,
              FontRole role) override;
    Size measure(std::string_view utf8, FontRole role) override;
    int line_height(FontRole role) override;
    void push_clip(const Rect& rect) override;
    void pop_clip() override;

    struct Font;

private:
    Font& font(FontRole role);
    const Font& font(FontRole role) const;

    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Font> ui_;
    std::unique_ptr<Font> ui_bold_;
    std::unique_ptr<Font> mono_;
    std::unique_ptr<Font> mono_small_;
    std::vector<SDL_Rect> clip_stack_;
};

}  // namespace kr8::ui::sdl
