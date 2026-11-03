#include <cstdint>
#include <string>
#include <string_view>

#include "harness.hpp"
#include "kr8/assembler.hpp"

using namespace kr8;

namespace {

AsmResult assemble_text(std::string_view text) {
    AsmOptions options;
    options.source_name = "test.asm";
    return assemble(text, options);
}

void check_ok(const AsmResult& result) {
    if (!result.ok) {
        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.line << ':' << diagnostic.column << ": "
                      << diagnostic.message << '\n';
        }
    }
    CHECK(result.ok);
}

}  // namespace

TEST(assembler_hello_program) {
    const AsmResult result = assemble_text(
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
        "msg: .asciz \"Hi\"\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }

    CHECK_EQ(result.code_bytes, 10u);
    CHECK_EQ(result.data_bytes, 3u);
    CHECK_EQ(result.image.entry, 0);
    CHECK_EQ(result.image.image_high, 0x0D);

    CHECK_EQ(result.image.ram[0x00], 0x0B);
    CHECK_EQ(result.image.ram[0x01], 0x0A);
    CHECK_EQ(result.image.ram[0x02], 0x12);
    CHECK_EQ(result.image.ram[0x03], 0x51);
    CHECK_EQ(result.image.ram[0x04], 0x09);
    CHECK_EQ(result.image.ram[0x05], 0x60);
    CHECK_EQ(result.image.ram[0x06], 0x44);
    CHECK_EQ(result.image.ram[0x07], 0x50);
    CHECK_EQ(result.image.ram[0x08], 0x02);
    CHECK_EQ(result.image.ram[0x09], 0x00);
    CHECK_EQ(result.image.ram[0x0A], 'H');
    CHECK_EQ(result.image.ram[0x0B], 'i');
    CHECK_EQ(result.image.ram[0x0C], 0x00);

    CHECK(result.image.is_code(0));
    CHECK(result.image.is_code(0x09));
    CHECK(!result.image.is_code(0x0A));
    CHECK_EQ(result.image.line_at(0), 2);
    CHECK_EQ(result.image.line_at(9), 10);

    CHECK_EQ(result.image.symbols.size(), 4u);
    const Symbol* start = result.image.find_symbol(0, SymbolKind::Code);
    CHECK(start != nullptr);
    if (start != nullptr) {
        CHECK_EQ(start->name, std::string("start"));
    }
    const Symbol* data = result.image.find_symbol(0x0A, SymbolKind::Data);
    CHECK(data != nullptr);
    if (data != nullptr) {
        CHECK_EQ(data->size, 3);
    }
    CHECK(result.listing.find("MOV B, #msg") != std::string::npos);
}

TEST(assembler_directives_and_org) {
    const AsmResult result = assemble_text(
        ".equ VALUE, 0x41\n"
        ".org 0x10\n"
        "start:\n"
        "  MOV A, #VALUE\n"
        "  .byte 1, 2, 0xFF\n"
        "  .word 0x1234\n"
        "  .ascii \"AB\"\n"
        "  .asciz \"C\"\n"
        "  .space 2\n"
        "  HLT\n"
        ".entry start\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }

    CHECK_EQ(result.image.entry, 0x10);
    CHECK_EQ(result.image.image_high, 0x1E);
    CHECK_EQ(result.image.ram[0x10], 0x0A);
    CHECK_EQ(result.image.ram[0x11], 0x41);
    CHECK_EQ(result.image.ram[0x12], 1);
    CHECK_EQ(result.image.ram[0x13], 2);
    CHECK_EQ(result.image.ram[0x14], 0xFF);
    CHECK_EQ(result.image.ram[0x15], 0x34);
    CHECK_EQ(result.image.ram[0x16], 0x12);
    CHECK_EQ(result.image.ram[0x17], 'A');
    CHECK_EQ(result.image.ram[0x18], 'B');
    CHECK_EQ(result.image.ram[0x19], 'C');
    CHECK_EQ(result.image.ram[0x1A], 0);
    CHECK_EQ(result.image.ram[0x1B], 0);
    CHECK_EQ(result.image.ram[0x1C], 0);
    CHECK_EQ(result.image.ram[0x1D], 0x00);

    const Symbol* value = result.image.find_symbol(0x41, SymbolKind::Const);
    CHECK(value != nullptr);
    if (value != nullptr) {
        CHECK_EQ(value->name, std::string("VALUE"));
    }
    const Symbol* start = result.image.find_symbol(0x10, SymbolKind::Code);
    CHECK(start != nullptr);
    if (start != nullptr) {
        CHECK_EQ(start->name, std::string("start"));
    }
}

TEST(assembler_data_label_size) {
    const AsmResult result = assemble_text(
        "start:\n"
        "  HLT\n"
        "table: .byte 1, 2, 3, 4\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }
    const Symbol* table = result.image.find_symbol(1, SymbolKind::Data);
    CHECK(table != nullptr);
    if (table != nullptr) {
        CHECK_EQ(table->name, std::string("table"));
        CHECK_EQ(table->size, 4);
    }
}

TEST(assembler_forward_reference) {
    const AsmResult result = assemble_text(
        "  JMP later\n"
        "  HLT\n"
        "later:\n"
        "  NOP\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }
    CHECK_EQ(result.image.ram[0], 0x50);
    CHECK_EQ(result.image.ram[1], 3);
}

TEST(assembler_expression_precedence) {
    const AsmResult result = assemble_text(
        ".equ VALUE, 2 + 3 * 4\n"
        "  MOV A, #VALUE\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }
    CHECK_EQ(result.image.ram[1], 14);
}

TEST(assembler_characters_and_escapes) {
    const AsmResult result = assemble_text(
        "  MOV A, #'A'\n"
        "  .byte '\\n', '\\x42', '\\\\'\n"
        "  .asciz \"a\\n\"\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }
    CHECK_EQ(result.image.ram[1], 'A');
    CHECK_EQ(result.image.ram[2], '\n');
    CHECK_EQ(result.image.ram[3], 'B');
    CHECK_EQ(result.image.ram[4], '\\');
    CHECK_EQ(result.image.ram[5], 'a');
    CHECK_EQ(result.image.ram[6], '\n');
    CHECK_EQ(result.image.ram[7], 0);
}

TEST(assembler_branch_aliases) {
    const AsmResult result = assemble_text(
        "  BEQ 0x10\n"
        "  BRA 0x20\n"
        "  BLT 0x30\n");
    check_ok(result);
    if (!result.ok) {
        return;
    }
    CHECK_EQ(result.image.ram[0], 0x51);
    CHECK_EQ(result.image.ram[2], 0x50);
    CHECK_EQ(result.image.ram[4], 0x59);
}

TEST(assembler_reports_undefined_symbol) {
    const AsmResult result = assemble_text("  JMP nowhere\n");
    CHECK(!result.ok);
    CHECK(!result.diagnostics.empty());
    if (!result.diagnostics.empty()) {
        CHECK(result.diagnostics[0].message.find("undefined") !=
              std::string::npos);
    }
}

TEST(assembler_reports_duplicate_label) {
    const AsmResult result = assemble_text(
        "start:\n"
        "  NOP\n"
        "start:\n"
        "  HLT\n");
    CHECK(!result.ok);
}

TEST(assembler_reports_backward_org) {
    const AsmResult result = assemble_text(
        ".org 0x10\n"
        ".org 0x08\n"
        "  NOP\n");
    CHECK(!result.ok);
}

TEST(assembler_rejects_two_byte_instruction_at_ff) {
    const AsmResult result = assemble_text(
        ".org 0xFF\n"
        "  MOV A, #1\n");
    CHECK(!result.ok);
}

TEST(assembler_reports_image_overflow) {
    const AsmResult result = assemble_text(".space 300\n");
    CHECK(!result.ok);
}

TEST(assembler_reports_label_without_data) {
    const AsmResult result = assemble_text("orphan:\n");
    CHECK(!result.ok);
}

TEST(assembler_reports_bad_byte_range) {
    const AsmResult result = assemble_text(
        "  NOP\n"
        "  .byte 300\n");
    CHECK(!result.ok);
}
