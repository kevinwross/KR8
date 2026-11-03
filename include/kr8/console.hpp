#pragma once

#include <cstdint>
#include <optional>

namespace kr8 {

/// Read one byte from stdin, blocking if necessary. Returns nullopt at EOF.
std::optional<std::uint8_t> read_stdin_byte();

}  // namespace kr8
