#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "kr8/image.hpp"

namespace kr8 {

struct DecodedInstruction {
    std::uint8_t address = 0;
    std::uint8_t size = 1;
    std::array<std::uint8_t, 2> bytes{};
    std::string text;
    int line = 0;
    bool valid = false;
};

struct DataRange {
    std::uint8_t address = 0;
    std::uint16_t size = 0;
    const Symbol* symbol = nullptr;
};

struct DecodedImage {
    std::vector<DecodedInstruction> instructions;
    std::vector<DataRange> data;
};

struct DisasmOptions {
    bool bytes = true;
    bool symbols = true;
    bool data = true;
    bool source = false;
};

/// Decode the code map into instructions and data ranges. Symbols are used for
/// branch targets and memory operands.
DecodedImage decode_image(const Image& image);

/// Decode raw bytes linearly from `base` to `end` (inclusive), marking
/// instruction starts as it goes. Useful for a `.bin` with no code map.
DecodedImage decode_raw(std::span<const std::uint8_t> bytes,
                        std::uint8_t base = 0, std::uint8_t end = 0xFF);

std::string disassemble_text(const Image& image,
                             const DisasmOptions& options = {});

/// Format an already-decoded image (used by raw mode and the UI).
std::string format_decoded(const Image& image, const DecodedImage& decoded,
                           const DisasmOptions& options = {});
std::string symbol_table_text(const Image& image);

}  // namespace kr8
