#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

#include "harness.hpp"
#include "kr8/isa.hpp"

using namespace kr8;

TEST(isa_has_exactly_63_instructions) {
    CHECK_EQ(opcode_table().size(), kInstructionCount);
    CHECK_EQ(kInstructionCount, 63u);
}

TEST(isa_opcodes_and_ops_are_unique) {
    std::set<std::uint8_t> opcodes;
    std::set<Op> ops;
    for (const auto& info : opcode_table()) {
        CHECK(opcodes.insert(info.opcode).second);
        CHECK(ops.insert(info.op).second);
        CHECK(info.op != Op::Count);
    }
    CHECK_EQ(opcodes.size(), kInstructionCount);
    CHECK_EQ(ops.size(), kInstructionCount);
}

TEST(isa_sizes_and_operands_are_consistent) {
    for (const auto& info : opcode_table()) {
        CHECK(info.size == 1 || info.size == 2);
        if (info.size == 1) {
            CHECK(info.operand == OperandKind::None);
            CHECK_EQ(info.suffix, std::string_view{});
        } else {
            CHECK(info.operand != OperandKind::None);
        }
        CHECK((info.flags & static_cast<std::uint8_t>(~FlagAll)) == 0);
    }
}

TEST(isa_metadata_is_present) {
    for (const auto& info : opcode_table()) {
        CHECK(!info.mnemonic.empty());
        CHECK(!info.base.empty());
        CHECK(!info.prefix.empty());
        CHECK(!info.help.empty());
        CHECK_EQ(info_for_opcode(info.opcode), &info);
        CHECK_EQ(info_for_op(info.op), &info);
    }
}

TEST(isa_aliases_resolve_to_canonical_ops) {
    CHECK_EQ(find_mnemonic("BEQ"), info_for_op(Op::Jz));
    CHECK_EQ(find_mnemonic("bne"), info_for_op(Op::Jnz));
    CHECK_EQ(find_mnemonic("BRA"), info_for_op(Op::Jmp));
    CHECK_EQ(find_mnemonic("BCS"), info_for_op(Op::Jc));
    CHECK_EQ(find_mnemonic("BHS"), info_for_op(Op::Jc));
    CHECK_EQ(find_mnemonic("BCC"), info_for_op(Op::Jnc));
    CHECK_EQ(find_mnemonic("BLO"), info_for_op(Op::Jnc));
    CHECK_EQ(find_mnemonic("BMI"), info_for_op(Op::Jn));
    CHECK_EQ(find_mnemonic("BPL"), info_for_op(Op::Jnn));
    CHECK_EQ(find_mnemonic("BVS"), info_for_op(Op::Jv));
    CHECK_EQ(find_mnemonic("BVC"), info_for_op(Op::Jnv));
    CHECK_EQ(find_mnemonic("BLT"), info_for_op(Op::Jlt));
    CHECK_EQ(find_mnemonic("BGE"), info_for_op(Op::Jge));
    CHECK_EQ(find_mnemonic("BGT"), info_for_op(Op::Jgt));
    CHECK_EQ(find_mnemonic("BLE"), info_for_op(Op::Jle));
}

TEST(isa_unknown_names_return_null) {
    CHECK(find_mnemonic("NOPE") == nullptr);
    CHECK(find_mnemonic("") == nullptr);
}

TEST(isa_reserved_opcodes_are_illegal) {
    const std::uint8_t reserved[] = {
        0x0C, 0x0F, 0x16, 0x1F, 0x29, 0x2F, 0x39,
        0x3F, 0x4B, 0x4F, 0x5E, 0x5F, 0x62, 0xFF,
    };
    for (const std::uint8_t opcode : reserved) {
        CHECK(info_for_opcode(opcode) == nullptr);
        CHECK_EQ(instruction_text(opcode, 0), std::string("???"));
    }
}

TEST(isa_instruction_text_formats_operands) {
    CHECK_EQ(instruction_text(0x00, 0), std::string("HLT"));
    CHECK_EQ(instruction_text(0x0A, 0x41), std::string("MOV A, #0x41"));
    CHECK_EQ(instruction_text(0x13, 0x20), std::string("ST [0x20], A"));
    CHECK_EQ(instruction_text(0x51, 0x0A), std::string("JZ 0x0A"));
    CHECK_EQ(instruction_text(0xFF, 0), std::string("???"));
}

TEST(isa_operand_formatting) {
    const auto* mov = info_for_op(Op::MovAImm);
    const auto* hlt = info_for_op(Op::Hlt);
    CHECK(mov != nullptr);
    CHECK(hlt != nullptr);
    if (mov == nullptr || hlt == nullptr) {
        return;
    }
    CHECK_EQ(format_operand(*mov, 0x05), std::string("0x05"));
    CHECK_EQ(format_operand(*hlt, 0), std::string(""));
    CHECK_EQ(format_instruction(*mov, "0x05"), std::string("MOV A, #0x05"));
}

TEST(isa_branch_classification) {
    CHECK(is_branch(Op::Jmp));
    CHECK(is_branch(Op::Jz));
    CHECK(is_branch(Op::Jle));
    CHECK(is_branch(Op::Call));
    CHECK(!is_branch(Op::AddB));
    CHECK(!is_branch(Op::Hlt));
}

TEST(isa_canonical_mnemonics) {
    CHECK_EQ(canonical_mnemonic(Op::Jz), std::string_view("JZ"));
    CHECK_EQ(canonical_mnemonic(Op::MovAImm), std::string_view("MOV"));
    CHECK_EQ(canonical_mnemonic(Op::Count), std::string_view("???"));
}

TEST(isa_help_lookup) {
    CHECK(!instruction_help("BEQ").empty());
    CHECK(!instruction_help(Op::LdAAddr).empty());
    CHECK(instruction_help("NOPE").empty());
}
