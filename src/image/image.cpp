#include "kr8/image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kr8 {
namespace {

constexpr std::uint16_t kFormatVersion = 0x0100;
constexpr std::size_t kHeaderSize = 32;
constexpr std::size_t kRamSize = 256;
constexpr std::size_t kCodeMapSize = 32;
constexpr std::size_t kSymbolSize = 12;
constexpr std::size_t kLineSize = 4;
constexpr std::size_t kSourceSize = 12;

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

bool read_u16(std::span<const std::uint8_t> data, std::size_t& offset,
              std::uint16_t& value) {
    if (offset + 2 > data.size()) {
        return false;
    }
    value = static_cast<std::uint16_t>(data[offset]) |
            static_cast<std::uint16_t>(data[offset + 1]) << 8;
    offset += 2;
    return true;
}

bool read_u32(std::span<const std::uint8_t> data, std::size_t& offset,
              std::uint32_t& value) {
    if (offset + 4 > data.size()) {
        return false;
    }
    value = static_cast<std::uint32_t>(data[offset]) |
            static_cast<std::uint32_t>(data[offset + 1]) << 8 |
            static_cast<std::uint32_t>(data[offset + 2]) << 16 |
            static_cast<std::uint32_t>(data[offset + 3]) << 24;
    offset += 4;
    return true;
}

bool read_cstring(std::span<const std::uint8_t> table, std::uint32_t offset,
                  std::string& out) {
    if (offset >= table.size()) {
        return false;
    }
    std::size_t end = offset;
    while (end < table.size() && table[end] != 0) {
        ++end;
    }
    if (end == table.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(table.data() + offset),
               end - offset);
    return true;
}

std::string make_error(std::string_view message) {
    return std::string(message);
}

}  // namespace

std::uint32_t crc32(std::span<const std::uint8_t> data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                static_cast<std::uint32_t>(-(static_cast<int>(crc & 1u)));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

const Symbol* Image::find_symbol(std::uint8_t address,
                                 std::optional<SymbolKind> kind) const {
    for (const auto& symbol : symbols) {
        if (kind.has_value() && symbol.kind != *kind) {
            continue;
        }
        if (symbol.value == static_cast<std::int32_t>(address)) {
            return &symbol;
        }
    }
    return nullptr;
}

const SourceLineRow* Image::find_line(std::uint8_t address) const {
    for (const auto& row : lines) {
        if (row.address == address) {
            return &row;
        }
    }
    return nullptr;
}

int Image::line_at(std::uint8_t address) const {
    const SourceLineRow* row = find_line(address);
    return row != nullptr ? static_cast<int>(row->line) : 0;
}

std::string_view Image::source_text() const {
    if (sources.empty()) {
        return {};
    }
    return sources.front().text;
}

std::vector<std::uint8_t> write_image(const Image& image) {
    std::string strings;
    std::unordered_map<std::string, std::uint16_t> string_offsets;
    bool overflow = false;

    auto intern = [&](const std::string& value) -> std::uint16_t {
        const auto found = string_offsets.find(value);
        if (found != string_offsets.end()) {
            return found->second;
        }
        if (strings.size() + value.size() + 1 > 0xFFFFu) {
            overflow = true;
            return 0;
        }
        const auto offset = static_cast<std::uint16_t>(strings.size());
        strings.append(value);
        strings.push_back('\0');
        string_offsets.emplace(value, offset);
        return offset;
    };

    std::string source_blob;
    std::vector<std::uint32_t> source_text_offsets;
    source_text_offsets.reserve(image.sources.size());
    for (const auto& source : image.sources) {
        source_text_offsets.push_back(
            static_cast<std::uint32_t>(source_blob.size()));
        source_blob.append(source.text);
    }

    if (image.symbols.size() > std::numeric_limits<std::uint16_t>::max() ||
        image.lines.size() > std::numeric_limits<std::uint16_t>::max() ||
        image.sources.size() > std::numeric_limits<std::uint16_t>::max()) {
        return {};
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(kRamSize + kCodeMapSize +
                    image.symbols.size() * kSymbolSize +
                    image.lines.size() * kLineSize +
                    image.sources.size() * kSourceSize +
                    strings.size() + source_blob.size() + 8);

    payload.insert(payload.end(), image.ram.begin(), image.ram.end());
    payload.insert(payload.end(), image.code_map.begin(), image.code_map.end());

    for (const auto& symbol : image.symbols) {
        const std::uint16_t name_offset = intern(symbol.name);
        append_u16(payload, name_offset);
        append_u16(payload, 0);
        append_u32(payload, static_cast<std::uint32_t>(symbol.value));
        payload.push_back(static_cast<std::uint8_t>(symbol.kind));
        payload.push_back(0);
        append_u16(payload, symbol.size);
    }

    for (const auto& row : image.lines) {
        payload.push_back(row.address);
        payload.push_back(row.file);
        append_u16(payload, row.line);
    }

    for (std::size_t i = 0; i < image.sources.size(); ++i) {
        const auto& source = image.sources[i];
        const std::uint16_t path_offset = intern(source.path);
        append_u16(payload, path_offset);
        append_u16(payload, 0);
        append_u32(payload, source_text_offsets[i]);
        append_u32(payload, static_cast<std::uint32_t>(source.text.size()));
    }

    append_u32(payload, static_cast<std::uint32_t>(strings.size()));
    payload.insert(payload.end(), strings.begin(), strings.end());
    append_u32(payload, static_cast<std::uint32_t>(source_blob.size()));
    payload.insert(payload.end(), source_blob.begin(), source_blob.end());

    if (overflow) {
        return {};
    }

    std::uint16_t flags = 0;
    if (!image.sources.empty()) {
        flags = static_cast<std::uint16_t>(flags | 1u);
    }
    if (!image.symbols.empty()) {
        flags = static_cast<std::uint16_t>(flags | 2u);
    }
    if (!image.lines.empty()) {
        flags = static_cast<std::uint16_t>(flags | 4u);
    }
    if (std::any_of(image.code_map.begin(), image.code_map.end(),
                    [](std::uint8_t byte) { return byte != 0; })) {
        flags = static_cast<std::uint16_t>(flags | 8u);
    }

    std::vector<std::uint8_t> output;
    output.reserve(kHeaderSize + payload.size());
    output.push_back('K');
    output.push_back('R');
    output.push_back('8');
    output.push_back(0);
    append_u16(output, kFormatVersion);
    append_u16(output, flags);
    output.push_back(image.entry);
    output.push_back(0);
    append_u16(output,
               static_cast<std::uint16_t>(std::min<std::uint16_t>(
                   image.image_high, 256)));
    append_u16(output,
               static_cast<std::uint16_t>(image.symbols.size()));
    append_u16(output, static_cast<std::uint16_t>(image.lines.size()));
    append_u16(output, static_cast<std::uint16_t>(image.sources.size()));
    append_u16(output, 0);
    append_u32(output, crc32(payload));
    for (int i = 0; i < 8; ++i) {
        output.push_back(0);
    }

    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}

ImageLoadResult read_image(std::span<const std::uint8_t> bytes) {
    ImageLoadResult result;
    if (bytes.size() < kHeaderSize) {
        result.error = make_error("image is shorter than the header");
        return result;
    }
    if (bytes[0] != 'K' || bytes[1] != 'R' || bytes[2] != '8' ||
        bytes[3] != 0) {
        result.error = make_error("bad image magic (expected KR8)");
        return result;
    }

    std::size_t offset = 4;
    std::uint16_t version = 0;
    std::uint16_t flags = 0;
    if (!read_u16(bytes, offset, version) ||
        !read_u16(bytes, offset, flags)) {
        result.error = make_error("truncated image header");
        return result;
    }
    if (version != kFormatVersion) {
        result.error = make_error("unsupported image format version");
        return result;
    }
    offset = 8;
    const std::uint8_t entry = bytes[offset++];
    ++offset;  // reserved
    std::uint16_t image_high = 0;
    std::uint16_t symbol_count = 0;
    std::uint16_t line_count = 0;
    std::uint16_t source_count = 0;
    std::uint16_t reserved16 = 0;
    if (!read_u16(bytes, offset, image_high) ||
        !read_u16(bytes, offset, symbol_count) ||
        !read_u16(bytes, offset, line_count) ||
        !read_u16(bytes, offset, source_count) ||
        !read_u16(bytes, offset, reserved16)) {
        result.error = make_error("truncated image counts");
        return result;
    }
    std::uint32_t stored_crc = 0;
    if (!read_u32(bytes, offset, stored_crc)) {
        result.error = make_error("truncated image checksum");
        return result;
    }
    if (image_high > 256) {
        result.error = make_error("image_high is out of range");
        return result;
    }

    const auto payload = bytes.subspan(kHeaderSize);
    if (crc32(payload) != stored_crc) {
        result.error = make_error("image checksum mismatch");
        return result;
    }

    const std::size_t fixed_arrays = kRamSize + kCodeMapSize;
    const std::size_t symbols_bytes =
        static_cast<std::size_t>(symbol_count) * kSymbolSize;
    const std::size_t lines_bytes =
        static_cast<std::size_t>(line_count) * kLineSize;
    const std::size_t sources_bytes =
        static_cast<std::size_t>(source_count) * kSourceSize;
    const std::size_t arrays_end =
        fixed_arrays + symbols_bytes + lines_bytes + sources_bytes;
    if (payload.size() < arrays_end + 4) {
        result.error = make_error("image payload is truncated");
        return result;
    }

    Image image;
    image.entry = entry;
    image.image_high = image_high;
    image.flags = flags;
    std::copy_n(payload.begin(), kRamSize, image.ram.begin());
    std::copy_n(payload.begin() + kRamSize, kCodeMapSize,
                image.code_map.begin());

    std::vector<std::uint16_t> symbol_name_offsets;
    symbol_name_offsets.reserve(symbol_count);
    std::size_t pos = kHeaderSize + fixed_arrays;
    for (std::uint16_t i = 0; i < symbol_count; ++i) {
        std::uint16_t name_offset = 0;
        std::uint16_t reserved = 0;
        std::uint32_t value = 0;
        if (!read_u16(bytes, pos, name_offset) ||
            !read_u16(bytes, pos, reserved) ||
            !read_u32(bytes, pos, value)) {
            result.error = make_error("truncated symbol record");
            return result;
        }
        if (pos + 4 > bytes.size()) {
            result.error = make_error("truncated symbol record");
            return result;
        }
        const auto kind = static_cast<SymbolKind>(bytes[pos++]);
        ++pos;  // padding
        std::uint16_t size = 0;
        if (!read_u16(bytes, pos, size)) {
            result.error = make_error("truncated symbol record");
            return result;
        }
        if (kind != SymbolKind::Code && kind != SymbolKind::Data &&
            kind != SymbolKind::Const) {
            result.error = make_error("invalid symbol kind");
            return result;
        }
        symbol_name_offsets.push_back(name_offset);
        image.symbols.push_back(Symbol{"", static_cast<std::int32_t>(value),
                                       kind, size});
    }

    std::vector<std::uint16_t> source_path_offsets;
    std::vector<std::uint32_t> source_text_offsets;
    std::vector<std::uint32_t> source_text_lengths;
    source_path_offsets.reserve(source_count);
    source_text_offsets.reserve(source_count);
    source_text_lengths.reserve(source_count);
    pos = kHeaderSize + fixed_arrays + symbols_bytes + lines_bytes;
    for (std::uint16_t i = 0; i < source_count; ++i) {
        std::uint16_t path_offset = 0;
        std::uint16_t reserved = 0;
        std::uint32_t text_offset = 0;
        std::uint32_t text_length = 0;
        if (!read_u16(bytes, pos, path_offset) ||
            !read_u16(bytes, pos, reserved) ||
            !read_u32(bytes, pos, text_offset) ||
            !read_u32(bytes, pos, text_length)) {
            result.error = make_error("truncated source record");
            return result;
        }
        source_path_offsets.push_back(path_offset);
        source_text_offsets.push_back(text_offset);
        source_text_lengths.push_back(text_length);
    }

    std::uint32_t string_table_size = 0;
    if (!read_u32(bytes, pos, string_table_size)) {
        result.error = make_error("missing string table size");
        return result;
    }
    if (pos + string_table_size > bytes.size()) {
        result.error = make_error("truncated string table");
        return result;
    }
    const auto string_table = bytes.subspan(pos, string_table_size);
    pos += string_table_size;

    std::uint32_t source_blob_size = 0;
    if (!read_u32(bytes, pos, source_blob_size)) {
        result.error = make_error("missing source blob size");
        return result;
    }
    if (pos + source_blob_size > bytes.size()) {
        result.error = make_error("truncated source blob");
        return result;
    }
    const auto source_blob = bytes.subspan(pos, source_blob_size);

    for (std::size_t i = 0; i < image.symbols.size(); ++i) {
        std::string name;
        if (!read_cstring(string_table, symbol_name_offsets[i], name)) {
            result.error = make_error("invalid symbol name offset");
            return result;
        }
        image.symbols[i].name = std::move(name);
    }

    for (std::uint16_t i = 0; i < source_count; ++i) {
        SourceFile source;
        if (!read_cstring(string_table, source_path_offsets[i],
                          source.path)) {
            result.error = make_error("invalid source path offset");
            return result;
        }
        const std::uint32_t text_offset = source_text_offsets[i];
        const std::uint32_t text_length = source_text_lengths[i];
        if (static_cast<std::uint64_t>(text_offset) + text_length >
            source_blob.size()) {
            result.error = make_error("source text is out of range");
            return result;
        }
        source.text.assign(
            reinterpret_cast<const char*>(source_blob.data() + text_offset),
            text_length);
        image.sources.push_back(std::move(source));
    }

    pos = kHeaderSize + fixed_arrays + symbols_bytes;
    for (std::uint16_t i = 0; i < line_count; ++i) {
        if (pos + kLineSize > bytes.size()) {
            result.error = make_error("truncated line table");
            return result;
        }
        SourceLineRow row;
        row.address = bytes[pos++];
        row.file = bytes[pos++];
        std::uint16_t line = 0;
        if (!read_u16(bytes, pos, line)) {
            result.error = make_error("truncated line table");
            return result;
        }
        row.line = line;
        if (!image.sources.empty() && row.file >= image.sources.size()) {
            result.error = make_error("line row has an invalid file index");
            return result;
        }
        image.lines.push_back(row);
    }

    result.ok = true;
    result.image = std::move(image);
    return result;
}

ImageLoadResult read_bin(std::span<const std::uint8_t> bytes) {
    ImageLoadResult result;
    if (bytes.size() > 256) {
        result.error = make_error("raw image is larger than 256 bytes");
        return result;
    }
    Image image;
    std::copy(bytes.begin(), bytes.end(), image.ram.begin());
    image.entry = 0;
    image.image_high = static_cast<std::uint16_t>(bytes.size());
    result.ok = true;
    result.image = std::move(image);
    return result;
}

std::vector<std::uint8_t> write_bin(const Image& image) {
    return std::vector<std::uint8_t>(image.ram.begin(), image.ram.end());
}

ImageLoadResult load_image_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return ImageLoadResult{false, {}, "cannot open image file"};
    }
    const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return read_image(bytes);
}

bool save_image_file(const std::filesystem::path& path, const Image& image,
                     std::string& error) {
    const std::vector<std::uint8_t> bytes = write_image(image);
    if (bytes.empty()) {
        error = "cannot serialize image";
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "cannot open output file";
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        error = "cannot write output file";
        return false;
    }
    return true;
}

bool save_bin_file(const std::filesystem::path& path, const Image& image,
                   std::string& error) {
    const std::vector<std::uint8_t> bytes = write_bin(image);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "cannot open output file";
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        error = "cannot write output file";
        return false;
    }
    return true;
}

}  // namespace kr8
