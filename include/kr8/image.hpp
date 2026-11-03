#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kr8 {

enum class SymbolKind : std::uint8_t {
    Code = 0,
    Data = 1,
    Const = 2,
};

struct Symbol {
    std::string name;
    std::int32_t value = 0;
    SymbolKind kind = SymbolKind::Code;
    std::uint16_t size = 0;
};

struct SourceLineRow {
    std::uint8_t address = 0;
    std::uint8_t file = 0;
    std::uint16_t line = 0;
};

struct SourceFile {
    std::string path;
    std::string text;
};

struct Image {
    std::array<std::uint8_t, 256> ram{};
    std::array<std::uint8_t, 32> code_map{};
    std::uint8_t entry = 0;
    std::uint16_t image_high = 0;
    std::vector<Symbol> symbols;
    std::vector<SourceLineRow> lines;
    std::vector<SourceFile> sources;
    std::uint16_t flags = 0;

    bool is_code(std::uint8_t address) const noexcept {
        return (code_map[address >> 3] & (1u << (address & 7))) != 0;
    }
    void mark_code(std::uint8_t address) noexcept {
        code_map[address >> 3] =
            static_cast<std::uint8_t>(code_map[address >> 3] |
                                      (1u << (address & 7)));
    }

    const Symbol* find_symbol(
        std::uint8_t address,
        std::optional<SymbolKind> kind = std::nullopt) const;
    const SourceLineRow* find_line(std::uint8_t address) const;
    int line_at(std::uint8_t address) const;
    std::string_view source_text() const;
};

std::uint32_t crc32(std::span<const std::uint8_t> data);

struct ImageLoadResult {
    bool ok = false;
    Image image;
    std::string error;
};

ImageLoadResult read_image(std::span<const std::uint8_t> bytes);
ImageLoadResult read_bin(std::span<const std::uint8_t> bytes);

std::vector<std::uint8_t> write_image(const Image& image);
std::vector<std::uint8_t> write_bin(const Image& image);

ImageLoadResult load_image_file(const std::filesystem::path& path);
bool save_image_file(const std::filesystem::path& path, const Image& image,
                     std::string& error);
bool save_bin_file(const std::filesystem::path& path, const Image& image,
                   std::string& error);

}  // namespace kr8
