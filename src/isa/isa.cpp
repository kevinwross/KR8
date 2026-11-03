#include "kr8/isa.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace kr8 {
namespace {

constexpr std::array<OpcodeInfo, kInstructionCount> kTable{{
    // System and stack.
    {0x00, Op::Hlt, "HLT", "HLT", "HLT", "", 1, OperandKind::None,
     FlagNone, "Stop the CPU normally."},
    {0x01, Op::Nop, "NOP", "NOP", "NOP", "", 1, OperandKind::None,
     FlagNone, "Do nothing."},
    {0x02, Op::Brk, "BRK", "BRK", "BRK", "", 1, OperandKind::None,
     FlagNone, "Halt with a breakpoint stop."},
    {0x03, Op::Ret, "RET", "RET", "RET", "", 1, OperandKind::None,
     FlagNone, "Return from a subroutine."},
    {0x04, Op::PushA, "PUSH A", "PUSH", "PUSH A", "", 1,
     OperandKind::None, FlagNone, "Push A onto the stack."},
    {0x05, Op::PushB, "PUSH B", "PUSH", "PUSH B", "", 1,
     OperandKind::None, FlagNone, "Push B onto the stack."},
    {0x06, Op::PopA, "POP A", "POP", "POP A", "", 1, OperandKind::None,
     FlagZ | FlagN, "Pop A from the stack."},
    {0x07, Op::PopB, "POP B", "POP", "POP B", "", 1, OperandKind::None,
     FlagZ | FlagN, "Pop B from the stack."},

    // Moves.
    {0x08, Op::MovAB, "MOV A, B", "MOV", "MOV A, B", "", 1,
     OperandKind::None, FlagZ | FlagN, "Copy B into A."},
    {0x09, Op::MovBA, "MOV B, A", "MOV", "MOV B, A", "", 1,
     OperandKind::None, FlagZ | FlagN, "Copy A into B."},
    {0x0A, Op::MovAImm, "MOV A, #n", "MOV", "MOV A, #", "", 2,
     OperandKind::Immediate, FlagZ | FlagN,
     "Load an 8-bit immediate into A."},
    {0x0B, Op::MovBImm, "MOV B, #n", "MOV", "MOV B, #", "", 2,
     OperandKind::Immediate, FlagZ | FlagN,
     "Load an 8-bit immediate into B."},

    // Loads and stores.
    {0x10, Op::LdAAddr, "LD A, [a]", "LD", "LD A, [", "]", 2,
     OperandKind::Address, FlagZ | FlagN,
     "Load A from an absolute memory address."},
    {0x11, Op::LdBAddr, "LD B, [a]", "LD", "LD B, [", "]", 2,
     OperandKind::Address, FlagZ | FlagN,
     "Load B from an absolute memory address."},
    {0x12, Op::LdAB, "LD A, [B]", "LD", "LD A, [B]", "", 1,
     OperandKind::None, FlagZ | FlagN,
     "Load A from the address in B."},
    {0x13, Op::StAAddr, "ST [a], A", "ST", "ST [", "], A", 2,
     OperandKind::Address, FlagNone,
     "Store A at an absolute memory address."},
    {0x14, Op::StBAddr, "ST [a], B", "ST", "ST [", "], B", 2,
     OperandKind::Address, FlagNone,
     "Store B at an absolute memory address."},
    {0x15, Op::StAB, "ST [B], A", "ST", "ST [B], A", "", 1,
     OperandKind::None, FlagNone,
     "Store A at the address in B."},

    // ALU with B.
    {0x20, Op::AddB, "ADD A, B", "ADD", "ADD A, B", "", 1,
     OperandKind::None, FlagAll, "Add B to A."},
    {0x21, Op::AdcB, "ADC A, B", "ADC", "ADC A, B", "", 1,
     OperandKind::None, FlagAll, "Add B and carry to A."},
    {0x22, Op::SubB, "SUB A, B", "SUB", "SUB A, B", "", 1,
     OperandKind::None, FlagAll, "Subtract B from A."},
    {0x23, Op::SbcB, "SBC A, B", "SBC", "SBC A, B", "", 1,
     OperandKind::None, FlagAll, "Subtract B and borrow from A."},
    {0x24, Op::CmpB, "CMP A, B", "CMP", "CMP A, B", "", 1,
     OperandKind::None, FlagAll,
     "Compare A with B and set flags without changing A."},
    {0x25, Op::AndB, "AND A, B", "AND", "AND A, B", "", 1,
     OperandKind::None, FlagAll, "AND B into A."},
    {0x26, Op::OrB, "OR A, B", "OR", "OR A, B", "", 1, OperandKind::None,
     FlagAll, "OR B into A."},
    {0x27, Op::XorB, "XOR A, B", "XOR", "XOR A, B", "", 1,
     OperandKind::None, FlagAll, "XOR B into A."},
    {0x28, Op::TstB, "TST A, B", "TST", "TST A, B", "", 1,
     OperandKind::None, FlagAll,
     "Test A AND B and set flags without changing A."},

    // ALU with an immediate.
    {0x30, Op::AddImm, "ADD A, #n", "ADD", "ADD A, #", "", 2,
     OperandKind::Immediate, FlagAll, "Add an 8-bit immediate to A."},
    {0x31, Op::AdcImm, "ADC A, #n", "ADC", "ADC A, #", "", 2,
     OperandKind::Immediate, FlagAll,
     "Add an immediate and carry to A."},
    {0x32, Op::SubImm, "SUB A, #n", "SUB", "SUB A, #", "", 2,
     OperandKind::Immediate, FlagAll,
     "Subtract an 8-bit immediate from A."},
    {0x33, Op::SbcImm, "SBC A, #n", "SBC", "SBC A, #", "", 2,
     OperandKind::Immediate, FlagAll,
     "Subtract an immediate and borrow from A."},
    {0x34, Op::CmpImm, "CMP A, #n", "CMP", "CMP A, #", "", 2,
     OperandKind::Immediate, FlagAll,
     "Compare A with an immediate and set flags."},
    {0x35, Op::AndImm, "AND A, #n", "AND", "AND A, #", "", 2,
     OperandKind::Immediate, FlagAll, "AND an immediate into A."},
    {0x36, Op::OrImm, "OR A, #n", "OR", "OR A, #", "", 2,
     OperandKind::Immediate, FlagAll, "OR an immediate into A."},
    {0x37, Op::XorImm, "XOR A, #n", "XOR", "XOR A, #", "", 2,
     OperandKind::Immediate, FlagAll, "XOR an immediate into A."},
    {0x38, Op::TstImm, "TST A, #n", "TST", "TST A, #", "", 2,
     OperandKind::Immediate, FlagAll,
     "Test A AND an immediate and set flags."},

    // Unary, shifts, and carry control.
    {0x40, Op::NotA, "NOT A", "NOT", "NOT A", "", 1, OperandKind::None,
     FlagAll, "Complement every bit of A."},
    {0x41, Op::NegA, "NEG A", "NEG", "NEG A", "", 1, OperandKind::None,
     FlagAll, "Negate A."},
    {0x42, Op::IncA, "INC A", "INC", "INC A", "", 1, OperandKind::None,
     FlagAll, "Increment A."},
    {0x43, Op::DecA, "DEC A", "DEC", "DEC A", "", 1, OperandKind::None,
     FlagAll, "Decrement A."},
    {0x44, Op::IncB, "INC B", "INC", "INC B", "", 1, OperandKind::None,
     FlagAll, "Increment B."},
    {0x45, Op::DecB, "DEC B", "DEC", "DEC B", "", 1, OperandKind::None,
     FlagAll, "Decrement B."},
    {0x46, Op::ShlA, "SHL A", "SHL", "SHL A", "", 1, OperandKind::None,
     FlagAll, "Shift A left; the old bit 7 goes to carry."},
    {0x47, Op::ShrA, "SHR A", "SHR", "SHR A", "", 1, OperandKind::None,
     FlagAll, "Shift A right; the old bit 0 goes to carry."},
    {0x48, Op::SarA, "SAR A", "SAR", "SAR A", "", 1, OperandKind::None,
     FlagAll, "Arithmetic shift A right; the sign bit is preserved."},
    {0x49, Op::Clc, "CLC", "CLC", "CLC", "", 1, OperandKind::None, FlagC,
     "Clear carry."},
    {0x4A, Op::Sec, "SEC", "SEC", "SEC", "", 1, OperandKind::None, FlagC,
     "Set carry."},

    // Branches and calls.
    {0x50, Op::Jmp, "JMP t", "JMP", "JMP ", "", 2, OperandKind::Target,
     FlagNone, "Jump to an absolute address."},
    {0x51, Op::Jz, "JZ t", "JZ", "JZ ", "", 2, OperandKind::Target,
     FlagNone, "Jump if Z is set."},
    {0x52, Op::Jnz, "JNZ t", "JNZ", "JNZ ", "", 2, OperandKind::Target,
     FlagNone, "Jump if Z is clear."},
    {0x53, Op::Jc, "JC t", "JC", "JC ", "", 2, OperandKind::Target,
     FlagNone, "Jump if C is set."},
    {0x54, Op::Jnc, "JNC t", "JNC", "JNC ", "", 2, OperandKind::Target,
     FlagNone, "Jump if C is clear."},
    {0x55, Op::Jn, "JN t", "JN", "JN ", "", 2, OperandKind::Target,
     FlagNone, "Jump if N is set."},
    {0x56, Op::Jnn, "JNN t", "JNN", "JNN ", "", 2, OperandKind::Target,
     FlagNone, "Jump if N is clear."},
    {0x57, Op::Jv, "JV t", "JV", "JV ", "", 2, OperandKind::Target,
     FlagNone, "Jump if V is set."},
    {0x58, Op::Jnv, "JNV t", "JNV", "JNV ", "", 2, OperandKind::Target,
     FlagNone, "Jump if V is clear."},
    {0x59, Op::Jlt, "JLT t", "JLT", "JLT ", "", 2, OperandKind::Target,
     FlagNone, "Jump if signed less (N != V)."},
    {0x5A, Op::Jge, "JGE t", "JGE", "JGE ", "", 2, OperandKind::Target,
     FlagNone, "Jump if signed greater or equal (N == V)."},
    {0x5B, Op::Jgt, "JGT t", "JGT", "JGT ", "", 2, OperandKind::Target,
     FlagNone, "Jump if signed greater (Z == 0 and N == V)."},
    {0x5C, Op::Jle, "JLE t", "JLE", "JLE ", "", 2, OperandKind::Target,
     FlagNone, "Jump if signed less or equal (Z == 1 or N != V)."},
    {0x5D, Op::Call, "CALL t", "CALL", "CALL ", "", 2,
     OperandKind::Target, FlagNone,
     "Call an absolute address; push the return address."},

    // Console I/O.
    {0x60, Op::OutA, "OUT A", "OUT", "OUT A", "", 1, OperandKind::None,
     FlagNone, "Send A to the console."},
    {0x61, Op::InA, "IN A", "IN", "IN A", "", 1, OperandKind::None, FlagZ,
     "Read a byte into A; Z is clear when a byte was available."},
}};

constexpr std::array<AliasEntry, 15> kAliases{{
    {"BRA", Op::Jmp},
    {"BEQ", Op::Jz},
    {"BNE", Op::Jnz},
    {"BCS", Op::Jc},
    {"BHS", Op::Jc},
    {"BCC", Op::Jnc},
    {"BLO", Op::Jnc},
    {"BMI", Op::Jn},
    {"BPL", Op::Jnn},
    {"BVS", Op::Jv},
    {"BVC", Op::Jnv},
    {"BLT", Op::Jlt},
    {"BGE", Op::Jge},
    {"BGT", Op::Jgt},
    {"BLE", Op::Jle},
}};

char upper_ascii(char value) {
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

bool equal_ci(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (upper_ascii(left[i]) != upper_ascii(right[i])) {
            return false;
        }
    }
    return true;
}

std::string hex8(std::uint8_t value) {
    constexpr char kDigits[] = "0123456789ABCDEF";
    std::string text;
    text.reserve(4);
    text += "0x";
    text += kDigits[(value >> 4) & 0x0F];
    text += kDigits[value & 0x0F];
    return text;
}

}  // namespace

std::span<const OpcodeInfo> opcode_table() {
    return kTable;
}

std::span<const AliasEntry> aliases() {
    return kAliases;
}

const OpcodeInfo* info_for_opcode(std::uint8_t opcode) {
    for (const auto& info : kTable) {
        if (info.opcode == opcode) {
            return &info;
        }
    }
    return nullptr;
}

const OpcodeInfo* info_for_op(Op op) {
    for (const auto& info : kTable) {
        if (info.op == op) {
            return &info;
        }
    }
    return nullptr;
}

const OpcodeInfo* find_mnemonic(std::string_view mnemonic) {
    for (const auto& info : kTable) {
        if (equal_ci(info.base, mnemonic)) {
            return &info;
        }
    }
    for (const auto& alias : kAliases) {
        if (equal_ci(alias.name, mnemonic)) {
            return info_for_op(alias.op);
        }
    }
    return nullptr;
}

std::string_view canonical_mnemonic(Op op) {
    const auto* info = info_for_op(op);
    return info != nullptr ? info->base : std::string_view("???");
}

std::string format_operand(const OpcodeInfo& info, std::uint8_t operand) {
    if (info.size == 1) {
        return {};
    }
    return hex8(operand);
}

std::string format_instruction(const OpcodeInfo& info,
                               std::string_view operand_text) {
    if (info.size == 1) {
        return std::string(info.mnemonic);
    }
    std::string text(info.prefix);
    text += operand_text;
    text += info.suffix;
    return text;
}

std::string instruction_text(std::uint8_t opcode, std::uint8_t operand) {
    const auto* info = info_for_opcode(opcode);
    if (info == nullptr) {
        return "???";
    }
    if (info->size == 1) {
        return std::string(info->mnemonic);
    }
    return format_instruction(*info, format_operand(*info, operand));
}

std::string_view instruction_help(std::string_view mnemonic) {
    const auto* info = find_mnemonic(mnemonic);
    return info != nullptr ? info->help : std::string_view{};
}

std::string_view instruction_help(Op op) {
    const auto* info = info_for_op(op);
    return info != nullptr ? info->help : std::string_view{};
}

bool is_branch(Op op) {
    switch (op) {
        case Op::Jmp:
        case Op::Jz:
        case Op::Jnz:
        case Op::Jc:
        case Op::Jnc:
        case Op::Jn:
        case Op::Jnn:
        case Op::Jv:
        case Op::Jnv:
        case Op::Jlt:
        case Op::Jge:
        case Op::Jgt:
        case Op::Jle:
        case Op::Call:
            return true;
        default:
            return false;
    }
}

}  // namespace kr8
