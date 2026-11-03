#pragma once

#include <string_view>

#include "kr8/ui/geometry.hpp"

namespace kr8::ui {

enum class FontRole {
    Ui,
    UiBold,
    Mono,
    MonoSmall,
};

/// A platform drawing surface. Coordinates are logical pixels; the backend
/// applies the display scale and owns all native handles.
///
/// Text is UTF-8. `text()` treats y as the top of the line box, matching the
/// existing GDI layout.
class Canvas {
public:
    virtual ~Canvas() = default;

    virtual void fill_rect(const Rect& rect, Color color) = 0;
    virtual void frame_rect(const Rect& rect, Color color,
                            int thickness = 1) = 0;
    virtual void text(int x, int y, std::string_view utf8, Color color,
                      FontRole font) = 0;
    virtual Size measure(std::string_view utf8, FontRole font) = 0;
    virtual int line_height(FontRole font) = 0;
    virtual void push_clip(const Rect& rect) = 0;
    virtual void pop_clip() = 0;
};

}  // namespace kr8::ui
