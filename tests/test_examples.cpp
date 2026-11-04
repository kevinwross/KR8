#include <filesystem>
#include <string>
#include <vector>

#include "harness.hpp"
#include "kr8/assembler.hpp"
#include "kr8/disassembler.hpp"
#include "kr8/image.hpp"

using namespace kr8;

TEST(examples_assemble_round_trip_and_disassemble) {
    const std::filesystem::path directory = KR8_EXAMPLES_DIR;
    const std::vector<std::string> names = {
        "hello.asm", "count.asm", "echo.asm", "puts.asm",
        "memory.asm", "stack.asm", "flags.asm", "updown.asm",
    };
    for (const auto& name : names) {
        const std::filesystem::path path = directory / name;
        AsmOptions options;
        options.source_name = path.string();
        const AsmResult result = assemble_file(path, options);
        CHECK(result.ok);
        if (!result.ok) {
            std::cerr << "assembly failed for " << name << '\n';
            continue;
        }
        CHECK(!result.image.symbols.empty());
        CHECK(result.image.image_high > 0);
        CHECK(result.image.image_high <= 256);

        const std::vector<std::uint8_t> bytes = write_image(result.image);
        CHECK(!bytes.empty());
        const ImageLoadResult loaded = read_image(bytes);
        CHECK(loaded.ok);
        if (loaded.ok) {
            CHECK_EQ(loaded.image.entry, result.image.entry);
            CHECK_EQ(loaded.image.image_high, result.image.image_high);
            CHECK_EQ(loaded.image.symbols.size(), result.image.symbols.size());
            CHECK_EQ(loaded.image.lines.size(), result.image.lines.size());
        }

        const std::string text = disassemble_text(result.image);
        CHECK(!text.empty());
        CHECK(text.find("Symbols:") != std::string::npos);
    }
}
