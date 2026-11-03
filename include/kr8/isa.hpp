#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace kr8 {

/// Every instruction in the KR8 ISA. Keep this list and the opcode table in
/// `src/isa/isa.cpp` in sync; the tests assert both count and uniqueness.
enum class Op : std::uint8_t {
    Hlt,
    Nop,
    Brk,
    Ret,
    PushA,
    PushB,
    PopA,
    PopB,
    MovAB,
    MovBA,
    MovAImm,
    MovBImm,
    LdAAddr,
    LdBAddr,
    LdAB,
    StAAddr,
    StBAddr,
    StAB,
    AddB,
    AdcB,
    SubB,
    SbcB,
    CmpB,
    AndB,
    OrB,
    XorB,
    TstB,
    AddImm,
    AdcImm,
    SubImm,
    SbcImm,
    CmpImm,
    AndImm,
    OrImm,
    XorImm,
    TstImm,
    NotA,
    NegA,
    IncA,
    DecA,
    IncB,
    DecB,
    ShlA,
    ShrA,
    SarA,
    Clc,
    Sec,
    Jmp,
    Jz,
    Jnz,
    Jc,
    Jnc,
    Jn,
    Jnn,
    Jv,
    Jnv,
    Jlt,
    Jge,
    Jgt,
    Jle,
    Call,
    OutA,
    InA,
    Count,
};

inline constexpr std::size_t kInstructionCount = 63;

/// The single operand byte of a 2-byte instruction, or None for a 1-byte
/// instruction.
enum class OperandKind : std::uint8_t {
    None,
    Immediate,  // #n
    Address,    // [a]
    Target,     // t
};

enum FlagMask : std::uint8_t {
    FlagNone = 0,
    FlagZ = 1u << 0,
    FlagC = 1u << 1,
    FlagN = 1u << 2,
    FlagV = 1u << 3,
    FlagAll = FlagZ | FlagC | FlagN | FlagV,
};

struct OpcodeInfo {
    std::uint8_t opcode = 0;
    Op op = Op::Hlt;
    std::string_view mnemonic;  // canonical full form, e.g. "MOV A, #n"
    std::string_view base;      // base mnemonic, e.g. "MOV"
    std::string_view prefix;    // text before the operand, e.g. "MOV A, #"
    std::string_view suffix;    // text after the operand, e.g. "]"
    std::uint8_t size = 1;
    OperandKind operand = OperandKind::None;
    std::uint8_t flags = FlagNone;  // flags whose value is defined afterwards
    std::string_view help;
};

struct AliasEntry {
    std::string_view name;
    Op op;
};

/// The opcode table, sorted by opcode. Exactly kInstructionCount entries.
std::span<const OpcodeInfo> opcode_table();

/// The canonical aliases accepted by the assembler (BRA, BEQ, ...).
std::span<const AliasEntry> aliases();

const OpcodeInfo* info_for_opcode(std::uint8_t opcode);
const OpcodeInfo* info_for_op(Op op);

/// Case-insensitive lookup by base mnemonic or alias. Returns the first entry
/// for multi-form bases such as MOV, which is fine for help and tooltips; the
/// assembler resolves the exact form from the operand syntax.
const OpcodeInfo* find_mnemonic(std::string_view mnemonic);

/// The base mnemonic for an Op, or "???" if invalid.
std::string_view canonical_mnemonic(Op op);

/// Format one operand byte as "0xNN". The prefix in the table supplies any
/// '#', '[' or ']' markers.
std::string format_operand(const OpcodeInfo& info, std::uint8_t operand);

/// Format a complete instruction with already-formatted operand text (used by
/// the symbol-aware disassembler).
std::string format_instruction(const OpcodeInfo& info,
                               std::string_view operand_text);

/// Format a raw instruction. Returns "???" for an illegal opcode.
std::string instruction_text(std::uint8_t opcode, std::uint8_t operand);

/// Short help text for a mnemonic or Op.
std::string_view instruction_help(std::string_view mnemonic);
std::string_view instruction_help(Op op);

/// True for instructions whose operand is a branch/call target.
bool is_branch(Op op);

}  // namespace kr8
