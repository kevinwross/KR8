#include "text_field.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <utility>

namespace kr8::ui {
namespace {

constexpr Color kFieldBg{35, 38, 47, 255};
constexpr Color kFieldBorder{52, 56, 68, 255};
constexpr Color kFieldFocus{82, 156, 255, 255};
constexpr Color kFieldText{214, 218, 226, 255};
constexpr Color kFieldDim{118, 124, 140, 255};
constexpr Color kFieldSelection{44, 74, 120, 255};
constexpr int kFieldPad = 8;

int selection_start(const TextField& field) {
    return std::min(field.anchor, field.cursor);
}

int selection_end(const TextField& field) {
    return std::max(field.anchor, field.cursor);
}

void erase_selection(TextField& field) {
    const int start = selection_start(field);
    const int end = selection_end(field);
    if (start == end) {
        return;
    }
    field.text.erase(static_cast<std::size_t>(start),
                     static_cast<std::size_t>(end - start));
    field.cursor = start;
    field.anchor = start;
}

int clamp_position(const TextField& field, int position) {
    return std::clamp(position, 0, static_cast<int>(field.text.size()));
}

int word_left(const std::string& text, int position) {
    while (position > 0 &&
           std::isspace(static_cast<unsigned char>(text[position - 1])) != 0) {
        --position;
    }
    while (position > 0 &&
           std::isspace(static_cast<unsigned char>(text[position - 1])) == 0) {
        --position;
    }
    return position;
}

int word_right(const std::string& text, int position) {
    const int size = static_cast<int>(text.size());
    while (position < size &&
           std::isspace(static_cast<unsigned char>(text[position])) == 0) {
        ++position;
    }
    while (position < size &&
           std::isspace(static_cast<unsigned char>(text[position])) != 0) {
        ++position;
    }
    return position;
}

std::string filter_input(std::string_view text) {
    std::string result;
    for (const char ch : text) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            result.push_back(' ');
        } else if (byte >= 128 || (byte >= 32 && byte != 127)) {
            result.push_back(ch);
        }
    }
    return result;
}

void insert_text(TextField& field, std::string_view text) {
    const std::string filtered = filter_input(text);
    if (filtered.empty()) {
        return;
    }
    erase_selection(field);
    field.text.insert(static_cast<std::size_t>(field.cursor), filtered);
    field.cursor += static_cast<int>(filtered.size());
    field.anchor = field.cursor;
}

}  // namespace

void text_field_set_text(TextField& field, std::string text) {
    field.text = std::move(text);
    field.cursor = static_cast<int>(field.text.size());
    field.anchor = field.cursor;
    field.scroll = 0;
}

void text_field_paint(Canvas& canvas, const Rect& rect, TextField& field,
                      std::string_view placeholder) {
    const Size digit = canvas.measure("0", FontRole::MonoSmall);
    field.char_width = std::max(1, digit.width);
    const int line_height = canvas.line_height(FontRole::MonoSmall);
    const int text_x = rect.left + kFieldPad;
    const int text_y = rect.top + (rect.height() - line_height) / 2;
    const int inner_width = std::max(1, rect.width() - 2 * kFieldPad);
    const int visible =
        std::max(1, inner_width / std::max(1, field.char_width));

    field.cursor = clamp_position(field, field.cursor);
    field.anchor = clamp_position(field, field.anchor);
    if (field.cursor < field.scroll) {
        field.scroll = field.cursor;
    } else if (field.cursor > field.scroll + visible) {
        field.scroll = field.cursor - visible;
    }
    field.scroll = std::max(0, field.scroll);

    canvas.fill_rect(rect, kFieldBg);
    canvas.frame_rect(rect, field.focused ? kFieldFocus : kFieldBorder);

    const int start = selection_start(field);
    const int end = selection_end(field);
    if (start != end) {
        const int left = std::clamp(
            text_x + (start - field.scroll) * field.char_width, text_x,
            rect.right - kFieldPad);
        const int right = std::clamp(
            text_x + (end - field.scroll) * field.char_width, text_x,
            rect.right - kFieldPad);
        if (right > left) {
            canvas.fill_rect(
                Rect{left, rect.top + 3, right, rect.bottom - 3},
                kFieldSelection);
        }
    }

    canvas.push_clip(Rect{text_x, rect.top + 2,
                          rect.right - kFieldPad, rect.bottom - 2});
    if (field.text.empty() && !field.focused) {
        canvas.text(text_x, text_y, std::string(placeholder), kFieldDim,
                    FontRole::MonoSmall);
    } else {
        canvas.text(text_x - field.scroll * field.char_width, text_y,
                    field.text, kFieldText, FontRole::MonoSmall);
    }
    if (field.focused) {
        const int caret_x =
            text_x + (field.cursor - field.scroll) * field.char_width;
        canvas.fill_rect(
            Rect{caret_x, rect.top + 3, caret_x + 1, rect.bottom - 3},
            kFieldFocus);
    }
    canvas.pop_clip();
}

bool text_field_text(TextField& field, std::string_view text) {
    if (text.empty()) {
        return false;
    }
    insert_text(field, text);
    return true;
}

bool text_field_key(TextField& field, const KeyDown& key, Window& window) {
    if (key.ctrl) {
        switch (key.key) {
            case Key::A:
                field.anchor = 0;
                field.cursor = static_cast<int>(field.text.size());
                return false;
            case Key::C: {
                const int start = selection_start(field);
                const int end = selection_end(field);
                if (start != end) {
                    window.set_clipboard_text(std::string_view(field.text)
                                                  .substr(start, end - start));
                }
                return false;
            }
            case Key::X: {
                const int start = selection_start(field);
                const int end = selection_end(field);
                if (start != end) {
                    window.set_clipboard_text(std::string_view(field.text)
                                                  .substr(start, end - start));
                    erase_selection(field);
                    return true;
                }
                return false;
            }
            case Key::V:
                insert_text(field, window.clipboard_text());
                return true;
            case Key::Left:
                field.cursor = word_left(field.text, field.cursor);
                if (!key.shift) {
                    field.anchor = field.cursor;
                }
                return false;
            case Key::Right:
                field.cursor = word_right(field.text, field.cursor);
                if (!key.shift) {
                    field.anchor = field.cursor;
                }
                return false;
            case Key::Backspace: {
                const int start = selection_start(field);
                const int end = selection_end(field);
                if (start != end) {
                    erase_selection(field);
                    return true;
                }
                const int target = word_left(field.text, field.cursor);
                field.text.erase(static_cast<std::size_t>(target),
                                 static_cast<std::size_t>(
                                     field.cursor - target));
                field.cursor = target;
                field.anchor = target;
                return true;
            }
            default:
                break;
        }
    }

    switch (key.key) {
        case Key::Left:
            if (field.cursor > 0) {
                --field.cursor;
            }
            if (!key.shift) {
                field.anchor = field.cursor;
            }
            return false;
        case Key::Right:
            field.cursor = clamp_position(field, field.cursor + 1);
            if (!key.shift) {
                field.anchor = field.cursor;
            }
            return false;
        case Key::Home:
            field.cursor = 0;
            if (!key.shift) {
                field.anchor = field.cursor;
            }
            return false;
        case Key::End:
            field.cursor = static_cast<int>(field.text.size());
            if (!key.shift) {
                field.anchor = field.cursor;
            }
            return false;
        case Key::Backspace: {
            const int start = selection_start(field);
            const int end = selection_end(field);
            if (start != end) {
                erase_selection(field);
                return true;
            }
            if (field.cursor > 0) {
                field.text.erase(
                    static_cast<std::size_t>(field.cursor - 1), 1);
                --field.cursor;
                field.anchor = field.cursor;
                return true;
            }
            return false;
        }
        case Key::Delete: {
            const int start = selection_start(field);
            const int end = selection_end(field);
            if (start != end) {
                erase_selection(field);
                return true;
            }
            if (field.cursor < static_cast<int>(field.text.size())) {
                field.text.erase(
                    static_cast<std::size_t>(field.cursor), 1);
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

void text_field_mouse_down(TextField& field, const Rect& rect, int x,
                           bool extend_selection) {
    field.focused = true;
    field.dragging = true;
    const int local = x - (rect.left + kFieldPad) +
                      field.scroll * field.char_width;
    const int position =
        clamp_position(field, local / std::max(1, field.char_width));
    field.cursor = position;
    if (!extend_selection) {
        field.anchor = position;
    }
}

void text_field_mouse_move(TextField& field, const Rect& rect, int x) {
    if (!field.dragging) {
        return;
    }
    const int local = x - (rect.left + kFieldPad) +
                      field.scroll * field.char_width;
    field.cursor =
        clamp_position(field, local / std::max(1, field.char_width));
}

void text_field_mouse_up(TextField& field) {
    field.dragging = false;
}

}  // namespace kr8::ui
