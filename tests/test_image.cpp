#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "harness.hpp"
#include "kr8/image.hpp"

using namespace kr8;

namespace {

Image make_image() {
    Image image;
    image.ram[0] = 0x0A;
    image.ram[1] = 0x41;
    image.ram[2] = 0x00;
    image.mark_code(0);
    image.mark_code(2);
    image.entry = 0;
    image.image_high = 3;
    image.symbols = {
        {"start", 0, SymbolKind::Code, 0},
        {"msg", 0x20, SymbolKind::Data, 5},
        {"ANSWER", 42, SymbolKind::Const, 0},
    };
    image.lines = {
        {0, 0, 1},
        {2, 0, 2},
    };
    image.sources = {
        {"hello.asm", "MOV A, #0x41\nHLT\n"},
    };
    return image;
}

}  // namespace

TEST(image_crc32_matches_known_vector) {
    const std::uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK_EQ(crc32(data), 0xCBF43926u);
}

TEST(image_round_trips) {
    const Image original = make_image();
    const std::vector<std::uint8_t> bytes = write_image(original);
    CHECK(!bytes.empty());

    const ImageLoadResult loaded = read_image(bytes);
    CHECK(loaded.ok);
    if (!loaded.ok) {
        return;
    }
    const Image& image = loaded.image;
    CHECK_EQ(image.entry, original.entry);
    CHECK_EQ(image.image_high, original.image_high);
    for (int address = 0; address < 256; ++address) {
        CHECK_EQ(image.ram[static_cast<std::size_t>(address)],
                 original.ram[static_cast<std::size_t>(address)]);
    }
    CHECK(image.is_code(0));
    CHECK(!image.is_code(1));
    CHECK(image.is_code(2));
    CHECK_EQ(image.symbols.size(), 3u);
    CHECK_EQ(image.lines.size(), 2u);
    CHECK_EQ(image.sources.size(), 1u);
    CHECK_EQ(image.sources[0].path, std::string("hello.asm"));
    CHECK_EQ(image.source_text(), std::string("MOV A, #0x41\nHLT\n"));
}

TEST(image_symbol_and_line_lookup) {
    const Image image = make_image();
    const Symbol* start = image.find_symbol(0, SymbolKind::Code);
    CHECK(start != nullptr);
    if (start != nullptr) {
        CHECK_EQ(start->name, std::string("start"));
    }
    const Symbol* data = image.find_symbol(0x20, SymbolKind::Data);
    CHECK(data != nullptr);
    if (data != nullptr) {
        CHECK_EQ(data->size, 5);
    }
    const Symbol* constant = image.find_symbol(42, SymbolKind::Const);
    CHECK(constant != nullptr);
    if (constant != nullptr) {
        CHECK_EQ(constant->name, std::string("ANSWER"));
    }
    CHECK_EQ(image.line_at(0), 1);
    CHECK_EQ(image.line_at(2), 2);
    CHECK_EQ(image.line_at(1), 0);
}

TEST(image_rejects_bad_magic) {
    std::vector<std::uint8_t> bytes = write_image(make_image());
    bytes[0] = 'X';
    const ImageLoadResult loaded = read_image(bytes);
    CHECK(!loaded.ok);
    CHECK(!loaded.error.empty());
}

TEST(image_rejects_checksum_mismatch) {
    std::vector<std::uint8_t> bytes = write_image(make_image());
    bytes[32] ^= 0xFF;
    const ImageLoadResult loaded = read_image(bytes);
    CHECK(!loaded.ok);
    CHECK(loaded.error.find("checksum") != std::string::npos);
}

TEST(image_rejects_truncated_input) {
    std::vector<std::uint8_t> bytes = write_image(make_image());
    bytes.resize(bytes.size() - 4);
    const ImageLoadResult loaded = read_image(bytes);
    CHECK(!loaded.ok);
}

TEST(raw_bin_round_trips_and_zero_fills) {
    const std::array<std::uint8_t, 4> raw{0x01, 0x02, 0x03, 0x04};
    const ImageLoadResult loaded = read_bin(raw);
    CHECK(loaded.ok);
    if (!loaded.ok) {
        return;
    }
    CHECK_EQ(loaded.image.ram[0], 0x01);
    CHECK_EQ(loaded.image.ram[3], 0x04);
    CHECK_EQ(loaded.image.ram[4], 0x00);
    CHECK_EQ(loaded.image.image_high, 4);
    const std::vector<std::uint8_t> bytes = write_bin(loaded.image);
    CHECK_EQ(bytes.size(), 256u);
}

TEST(raw_bin_rejects_oversized_input) {
    const std::array<std::uint8_t, 257> raw{};
    const ImageLoadResult loaded = read_bin(raw);
    CHECK(!loaded.ok);
}

TEST(image_file_round_trip) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "kr8_image_round_trip.k8";
    std::string error;
    CHECK(save_image_file(path, make_image(), error));
    const ImageLoadResult loaded = load_image_file(path);
    CHECK(loaded.ok);
    if (loaded.ok) {
        CHECK_EQ(loaded.image.entry, 0);
        CHECK_EQ(loaded.image.ram[1], 0x41);
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
