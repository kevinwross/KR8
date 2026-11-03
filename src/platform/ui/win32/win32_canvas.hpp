#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>

#include "kr8/ui/canvas.hpp"

namespace kr8::ui::win32 {

class Win32Canvas final : public Canvas {
public:
    Win32Canvas(HDC dc, RECT client);
    ~Win32Canvas() override;

    Win32Canvas(const Win32Canvas&) = delete;
    Win32Canvas& operator=(const Win32Canvas&) = delete;

    void fill_rect(const Rect& rect, Color color) override;
    void frame_rect(const Rect& rect, Color color, int thickness) override;
    void text(int x, int y, std::string_view utf8, Color color,
              FontRole font) override;
    Size measure(std::string_view utf8, FontRole font) override;
    int line_height(FontRole font) override;
    void push_clip(const Rect& rect) override;
    void pop_clip() override;

private:
    HFONT font(FontRole role) const;
    RECT to_native(const Rect& rect) const;

    HDC dc_ = nullptr;
    RECT client_{};
    HFONT ui_ = nullptr;
    HFONT ui_bold_ = nullptr;
    HFONT mono_ = nullptr;
    HFONT mono_small_ = nullptr;
    std::vector<int> saved_dc_;
};

}  // namespace kr8::ui::win32
