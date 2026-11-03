#pragma once

#include <cstdint>

namespace kr8::ui {

struct Point {
    int x = 0;
    int y = 0;

    friend bool operator==(Point, Point) = default;
};

struct Size {
    int width = 0;
    int height = 0;

    friend bool operator==(Size, Size) = default;
};

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    constexpr int width() const { return right - left; }
    constexpr int height() const { return bottom - top; }
    constexpr bool empty() const {
        return right <= left || bottom <= top;
    }
    constexpr bool contains(Point point) const {
        return point.x >= left && point.x < right && point.y >= top &&
               point.y < bottom;
    }

    friend bool operator==(Rect, Rect) = default;
};

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend bool operator==(Color, Color) = default;
};

constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return Color{r, g, b, 255};
}

constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                     std::uint8_t a) {
    return Color{r, g, b, a};
}

}  // namespace kr8::ui
