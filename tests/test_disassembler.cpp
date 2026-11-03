#include <cstdint>
#include <string>

#include "harness.hpp"
#include "kr8/assembler.hpp"
#include "kr8/disassembler.hpp"

using namespace kr8;

namespace {

AsmResult assemble_hello() {
    AsmOptions options;
    options.source_name = "hello.asm";
    return assemble(
        "start:\n"
        "  MOV B, #msg\n"
        "loop:\n"
        "  LD A, [B]\n"
        "  JZ done\n"
        "  OUT A\n"
        "  INC B\n"
        "  JMP loop\n"
        "done:\n"
        "  HLT\n"
        "msg: .asciz \"Hi\"\n",
        options);
}

}  // namespace

TEST(disassembler_decodes_code_and_data) {
    const AsmResult result = assemble_hello();
    CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const DecodedImage decoded = decode_image(result.image);
    CHECK_EQ(decoded.instructions.size(), 7u);
    if (decoded.instructions.size() != 7) {
        return;
    }
    CHECK_EQ(decoded.instructions[0].address, 0);
    CHECK_EQ(decoded.instructions[0].text, std::string("MOV B, #msg"));
    CHECK_EQ(decoded.instructions[1].text, std::string("LD A, [B]"));
    CHECK_EQ(decoded.instructions[2].text, std::string("JZ done"));
    CHECK_EQ(decoded.instructions[5].text, std::string("JMP loop"));
    CHECK(!decoded.data.empty());
    if (!decoded.data.empty()) {
        CHECK_EQ(decoded.data.back().address, 0x0A);
        CHECK_EQ(decoded.data.back().size, 3);
    }
}

TEST(disassembler_text_uses_symbols) {
    const AsmResult result = assemble_hello();
    CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    DisasmOptions options;
    options.symbols = true;
    options.data = true;
    const std::string text = disassemble_text(result.image, options);
    CHECK(text.find("JZ done") != std::string::npos);
    CHECK(text.find("JMP loop") != std::string::npos);
    CHECK(text.find(".asciz \"Hi\"") != std::string::npos);
    CHECK(text.find("Symbols:") != std::string::npos);
    CHECK(text.find("start") != std::string::npos);
}

TEST(disassembler_decodes_raw_without_symbols) {
    Image image;
    image.ram[0] = 0x0A;
    image.ram[1] = 0x41;
    image.ram[2] = 0x60;
    image.mark_code(0);
    image.mark_code(2);
    const DecodedImage decoded = decode_image(image);
    CHECK_EQ(decoded.instructions.size(), 2u);
    if (decoded.instructions.size() == 2) {
        CHECK_EQ(decoded.instructions[0].text, std::string("MOV A, #0x41"));
        CHECK_EQ(decoded.instructions[1].text, std::string("OUT A"));
    }
}

TEST(disassembler_marks_illegal_opcode) {
    Image image;
    image.ram[0] = 0x0C;
    image.mark_code(0);
    const DecodedImage decoded = decode_image(image);
    CHECK_EQ(decoded.instructions.size(), 1u);
    if (!decoded.instructions.empty()) {
        CHECK(!decoded.instructions[0].valid);
        CHECK_EQ(decoded.instructions[0].text, std::string("???"));
    }
}
