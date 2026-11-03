#include "kr8/console.hpp"

#include <cstdio>

namespace kr8 {

std::optional<std::uint8_t> read_stdin_byte() {
    const int value = std::getchar();
    if (value == EOF) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(value);
}

}  // namespace kr8
