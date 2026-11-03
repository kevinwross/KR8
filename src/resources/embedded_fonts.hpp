#pragma once

#include <cstdint>
#include <span>

namespace kr8::ui {

struct EmbeddedFonts {
    std::span<const std::uint8_t> ui_regular;
    std::span<const std::uint8_t> ui_bold;
    std::span<const std::uint8_t> mono_regular;
    std::span<const std::uint8_t> mono_bold;
};

EmbeddedFonts embedded_fonts();

}  // namespace kr8::ui
