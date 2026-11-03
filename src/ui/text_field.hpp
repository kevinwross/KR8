#pragma once

#include <string>
#include <string_view>

#include "kr8/ui/canvas.hpp"
#include "kr8/ui/input.hpp"
#include "kr8/ui/window.hpp"

namespace kr8::ui {

/// A small portable single-line text field. It supports selection, caret
/// movement, typing, and clipboard copy/cut/paste through Window.
struct TextField {
    std::string text;
    int cursor = 0;
    int anchor = 0;
    int scroll = 0;
    int char_width = 7;
    bool focused = false;
    bool dragging = false;
};

void text_field_set_text(TextField& field, std::string text);
void text_field_paint(Canvas& canvas, const Rect& rect, TextField& field,
                      std::string_view placeholder);
bool text_field_text(TextField& field, std::string_view text);
bool text_field_key(TextField& field, const KeyDown& key, Window& window);
void text_field_mouse_down(TextField& field, const Rect& rect, int x,
                           bool extend_selection);
void text_field_mouse_move(TextField& field, const Rect& rect, int x);
void text_field_mouse_up(TextField& field);

}  // namespace kr8::ui
