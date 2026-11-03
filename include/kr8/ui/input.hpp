#pragma once

#include <string>
#include <variant>

#include "kr8/ui/geometry.hpp"

namespace kr8::ui {

enum class MouseButton {
    None,
    Left,
    Middle,
    Right,
};

enum class Key {
    Unknown,
    Enter,
    Escape,
    Tab,
    Backspace,
    Delete,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    F1,
    F5,
    F6,
    F7,
    F8,
    F10,
    F11,
    A,
    C,
    O,
    R,
    S,
    V,
    X,
    Y,
    Z,
};

struct MouseMove {
    Point position;
};

struct MouseDown {
    Point position;
    MouseButton button = MouseButton::None;
    bool shift = false;
    bool ctrl = false;
};

struct MouseUp {
    Point position;
    MouseButton button = MouseButton::None;
};

struct MouseWheel {
    Point position;
    int delta = 0;
};

struct MouseLeave {};

struct KeyDown {
    Key key = Key::Unknown;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct KeyUp {
    Key key = Key::Unknown;
};

struct TextInput {
    std::string utf8;
};

struct Resize {
    Size size;
};

struct Focus {
    bool focused = false;
};

struct Timer {};

struct CloseRequest {};

using Event = std::variant<MouseMove, MouseDown, MouseUp, MouseWheel,
                           MouseLeave, KeyDown, KeyUp, TextInput, Resize,
                           Focus, Timer, CloseRequest>;

}  // namespace kr8::ui
