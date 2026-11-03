#include "kr8/cpu.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace kr8 {
namespace {

struct AluResult {
    std::uint8_t value = 0;
    bool carry = false;
    bool overflow = false;
};

AluResult add8(std::uint8_t left, std::uint8_t right, bool carry_in) {
    const std::uint16_t sum = static_cast<std::uint16_t>(left) + right +
                              (carry_in ? 1u : 0u);
    const auto value = static_cast<std::uint8_t>(sum & 0xFFu);
    const bool carry = sum > 0xFFu;
    const bool overflow =
        ((left ^ value) & (right ^ value) & 0x80u) != 0;
    return AluResult{value, carry, overflow};
}

AluResult sub8(std::uint8_t left, std::uint8_t right, bool borrow_in) {
    const int difference = static_cast<int>(left) - static_cast<int>(right) -
                           (borrow_in ? 1 : 0);
    const auto value = static_cast<std::uint8_t>(difference);
    const bool carry = difference >= 0;
    const bool overflow =
        ((left ^ right) & (left ^ value) & 0x80u) != 0;
    return AluResult{value, carry, overflow};
}

std::string hex_byte(std::uint8_t value) {
    constexpr char kDigits[] = "0123456789ABCDEF";
    std::string text = "0x";
    text += kDigits[(value >> 4) & 0x0F];
    text += kDigits[value & 0x0F];
    return text;
}

}  // namespace

Memory::Memory() {
    bytes_.fill(0);
}

void Memory::reset() {
    bytes_.fill(0);
}

void Memory::load(std::span<const std::uint8_t> image) {
    reset();
    const std::size_t count = std::min<std::size_t>(image.size(), 256);
    std::copy_n(image.begin(), count, bytes_.begin());
}

Cpu::Cpu() = default;

void Cpu::reset(std::uint8_t entry, std::uint16_t stack_floor) {
    a_ = 0;
    b_ = 0;
    pc_ = entry;
    sp_ = 0xFF;
    flags_ = 0;
    stack_depth_ = 0;
    stack_floor_ = stack_floor;
    halted_ = false;
    halt_kind_ = HaltKind::None;
    fault_ = FaultKind::None;
    fault_message_.clear();
    instructions_ = 0;
    cycles_ = 0;
    last_opcode_ = 0;
    last_operand_ = 0;
}

bool Cpu::resume_from_break() {
    if (!halted_ || halt_kind_ != HaltKind::Break) {
        return false;
    }
    pc_ = static_cast<std::uint8_t>(pc_ + 1);
    halted_ = false;
    halt_kind_ = HaltKind::None;
    return true;
}

StepResult Cpu::current_stop() const {
    StepResult result;
    if (fault_ != FaultKind::None) {
        result.status = StepStatus::Fault;
        result.fault = fault_;
        result.pc = pc_;
        result.message = fault_message_;
        return result;
    }
    if (halted_) {
        result.status = halt_kind_ == HaltKind::Break ? StepStatus::Break
                                                      : StepStatus::Halted;
        result.pc = pc_;
        result.message =
            halt_kind_ == HaltKind::Break ? "BRK" : "HLT";
    }
    return result;
}

StepResult Cpu::step_ok(std::uint8_t opcode, std::uint8_t operand,
                        std::uint32_t cycles) {
    ++instructions_;
    cycles_ += cycles;
    last_opcode_ = opcode;
    last_operand_ = operand;

    StepResult result;
    result.status = StepStatus::Ok;
    result.pc = pc_;
    result.cycles = cycles;
    result.opcode = opcode;
    result.operand = operand;
    return result;
}

StepResult Cpu::step_halt(std::uint8_t opcode, std::uint8_t operand,
                          std::uint32_t cycles) {
    halted_ = true;
    halt_kind_ = HaltKind::Hlt;
    ++instructions_;
    cycles_ += cycles;
    last_opcode_ = opcode;
    last_operand_ = operand;

    StepResult result;
    result.status = StepStatus::Halted;
    result.pc = pc_;
    result.cycles = cycles;
    result.opcode = opcode;
    result.operand = operand;
    result.message = "HLT";
    return result;
}

StepResult Cpu::step_break(std::uint8_t instruction_pc,
                           std::uint8_t opcode, std::uint32_t cycles) {
    halted_ = true;
    halt_kind_ = HaltKind::Break;
    pc_ = instruction_pc;
    ++instructions_;
    cycles_ += cycles;
    last_opcode_ = opcode;
    last_operand_ = 0;

    StepResult result;
    result.status = StepStatus::Break;
    result.pc = instruction_pc;
    result.cycles = cycles;
    result.opcode = opcode;
    result.message = "BRK at " + hex_byte(instruction_pc);
    return result;
}

StepResult Cpu::step_fault(FaultKind kind, std::uint8_t instruction_pc,
                           std::uint8_t opcode, std::uint8_t operand,
                           std::string message) {
    fault_ = kind;
    fault_message_ = std::move(message);
    pc_ = instruction_pc;

    StepResult result;
    result.status = StepStatus::Fault;
    result.fault = kind;
    result.pc = instruction_pc;
    result.opcode = opcode;
    result.operand = operand;
    result.message = fault_message_;
    return result;
}

bool Cpu::push(Memory& memory, std::uint8_t value) {
    if (stack_depth_ >= 256 || sp_ < stack_floor_) {
        return false;
    }
    memory.write8(sp_, value);
    sp_ = static_cast<std::uint8_t>(sp_ - 1);
    ++stack_depth_;
    return true;
}

bool Cpu::pop(Memory& memory, std::uint8_t& value) {
    if (stack_depth_ == 0) {
        return false;
    }
    sp_ = static_cast<std::uint8_t>(sp_ + 1);
    value = memory.read8(sp_);
    --stack_depth_;
    return true;
}

void Cpu::set_zn(std::uint8_t value) {
    flags_ = static_cast<std::uint8_t>(
        (flags_ & static_cast<std::uint8_t>(~(FlagZ | FlagN))) |
        (value == 0 ? FlagZ : 0) |
        ((value & 0x80u) != 0 ? FlagN : 0));
}

void Cpu::set_alu_flags(std::uint8_t value, bool carry, bool overflow) {
    flags_ = static_cast<std::uint8_t>(
        (value == 0 ? FlagZ : 0) | (carry ? FlagC : 0) |
        ((value & 0x80u) != 0 ? FlagN : 0) |
        (overflow ? FlagV : 0));
}

bool Cpu::condition(Op op) const {
    const bool z = flag(FlagZ);
    const bool c = flag(FlagC);
    const bool n = flag(FlagN);
    const bool v = flag(FlagV);
    switch (op) {
        case Op::Jz: return z;
        case Op::Jnz: return !z;
        case Op::Jc: return c;
        case Op::Jnc: return !c;
        case Op::Jn: return n;
        case Op::Jnn: return !n;
        case Op::Jv: return v;
        case Op::Jnv: return !v;
        case Op::Jlt: return n != v;
        case Op::Jge: return n == v;
        case Op::Jgt: return !z && (n == v);
        case Op::Jle: return z || (n != v);
        default: return false;
    }
}

StepResult Cpu::step(Memory& memory) {
    if (fault_ != FaultKind::None || halted_) {
        return current_stop();
    }

    const std::uint8_t instruction_pc = pc_;
    const std::uint8_t opcode = memory.read8(pc_);
    pc_ = static_cast<std::uint8_t>(pc_ + 1);

    const OpcodeInfo* info = info_for_opcode(opcode);
    if (info == nullptr) {
        return step_fault(FaultKind::IllegalOpcode, instruction_pc, opcode, 0,
                          "illegal opcode " + hex_byte(opcode) + " at " +
                              hex_byte(instruction_pc));
    }

    std::uint8_t operand = 0;
    if (info->size == 2) {
        if (instruction_pc == 0xFF) {
            return step_fault(FaultKind::PcWrap, instruction_pc, opcode, 0,
                              "instruction crosses end of memory at 0xFF");
        }
        operand = memory.read8(pc_);
        pc_ = static_cast<std::uint8_t>(pc_ + 1);
    }

    const std::uint8_t next_pc = pc_;
    std::uint32_t cycles = info->size;

    switch (info->op) {
        case Op::Hlt:
            return step_halt(opcode, operand, cycles);
        case Op::Nop:
            return step_ok(opcode, operand, cycles);
        case Op::Brk:
            return step_break(instruction_pc, opcode, cycles);
        case Op::Ret: {
            std::uint8_t value = 0;
            if (!pop(memory, value)) {
                return step_fault(FaultKind::StackUnderflow, instruction_pc,
                                  opcode, operand,
                                  "stack underflow at " +
                                      hex_byte(instruction_pc));
            }
            ++cycles;
            pc_ = value;
            return step_ok(opcode, operand, cycles);
        }
        case Op::PushA:
            if (!push(memory, a_)) {
                return step_fault(FaultKind::StackOverflow, instruction_pc,
                                  opcode, operand,
                                  "stack overflow at " +
                                      hex_byte(instruction_pc));
            }
            ++cycles;
            return step_ok(opcode, operand, cycles);
        case Op::PushB:
            if (!push(memory, b_)) {
                return step_fault(FaultKind::StackOverflow, instruction_pc,
                                  opcode, operand,
                                  "stack overflow at " +
                                      hex_byte(instruction_pc));
            }
            ++cycles;
            return step_ok(opcode, operand, cycles);
        case Op::PopA: {
            std::uint8_t value = 0;
            if (!pop(memory, value)) {
                return step_fault(FaultKind::StackUnderflow, instruction_pc,
                                  opcode, operand,
                                  "stack underflow at " +
                                      hex_byte(instruction_pc));
            }
            ++cycles;
            a_ = value;
            set_zn(a_);
            return step_ok(opcode, operand, cycles);
        }
        case Op::PopB: {
            std::uint8_t value = 0;
            if (!pop(memory, value)) {
                return step_fault(FaultKind::StackUnderflow, instruction_pc,
                                  opcode, operand,
                                  "stack underflow at " +
                                      hex_byte(instruction_pc));
            }
            ++cycles;
            b_ = value;
            set_zn(b_);
            return step_ok(opcode, operand, cycles);
        }
        case Op::MovAB:
            a_ = b_;
            set_zn(a_);
            return step_ok(opcode, operand, cycles);
        case Op::MovBA:
            b_ = a_;
            set_zn(b_);
            return step_ok(opcode, operand, cycles);
        case Op::MovAImm:
            a_ = operand;
            set_zn(a_);
            return step_ok(opcode, operand, cycles);
        case Op::MovBImm:
            b_ = operand;
            set_zn(b_);
            return step_ok(opcode, operand, cycles);
        case Op::LdAAddr:
            a_ = memory.read8(operand);
            ++cycles;
            set_zn(a_);
            return step_ok(opcode, operand, cycles);
        case Op::LdBAddr:
            b_ = memory.read8(operand);
            ++cycles;
            set_zn(b_);
            return step_ok(opcode, operand, cycles);
        case Op::LdAB:
            a_ = memory.read8(b_);
            ++cycles;
            set_zn(a_);
            return step_ok(opcode, operand, cycles);
        case Op::StAAddr:
            memory.write8(operand, a_);
            ++cycles;
            return step_ok(opcode, operand, cycles);
        case Op::StBAddr:
            memory.write8(operand, b_);
            ++cycles;
            return step_ok(opcode, operand, cycles);
        case Op::StAB:
            memory.write8(b_, a_);
            ++cycles;
            return step_ok(opcode, operand, cycles);
        case Op::AddB: {
            const AluResult result = add8(a_, b_, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::AdcB: {
            const AluResult result = add8(a_, b_, flag(FlagC));
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::SubB: {
            const AluResult result = sub8(a_, b_, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::SbcB: {
            const AluResult result = sub8(a_, b_, !flag(FlagC));
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::CmpB: {
            const AluResult result = sub8(a_, b_, false);
            set_alu_flags(result.value, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::AndB:
            a_ = static_cast<std::uint8_t>(a_ & b_);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::OrB:
            a_ = static_cast<std::uint8_t>(a_ | b_);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::XorB:
            a_ = static_cast<std::uint8_t>(a_ ^ b_);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::TstB:
            set_alu_flags(static_cast<std::uint8_t>(a_ & b_), false, false);
            return step_ok(opcode, operand, cycles);
        case Op::AddImm: {
            const AluResult result = add8(a_, operand, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::AdcImm: {
            const AluResult result = add8(a_, operand, flag(FlagC));
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::SubImm: {
            const AluResult result = sub8(a_, operand, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::SbcImm: {
            const AluResult result = sub8(a_, operand, !flag(FlagC));
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::CmpImm: {
            const AluResult result = sub8(a_, operand, false);
            set_alu_flags(result.value, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::AndImm:
            a_ = static_cast<std::uint8_t>(a_ & operand);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::OrImm:
            a_ = static_cast<std::uint8_t>(a_ | operand);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::XorImm:
            a_ = static_cast<std::uint8_t>(a_ ^ operand);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::TstImm:
            set_alu_flags(static_cast<std::uint8_t>(a_ & operand), false,
                          false);
            return step_ok(opcode, operand, cycles);
        case Op::NotA:
            a_ = static_cast<std::uint8_t>(~a_);
            set_alu_flags(a_, false, false);
            return step_ok(opcode, operand, cycles);
        case Op::NegA: {
            const AluResult result = sub8(0, a_, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::IncA: {
            const AluResult result = add8(a_, 1, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::DecA: {
            const AluResult result = sub8(a_, 1, false);
            a_ = result.value;
            set_alu_flags(a_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::IncB: {
            const AluResult result = add8(b_, 1, false);
            b_ = result.value;
            set_alu_flags(b_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::DecB: {
            const AluResult result = sub8(b_, 1, false);
            b_ = result.value;
            set_alu_flags(b_, result.carry, result.overflow);
            return step_ok(opcode, operand, cycles);
        }
        case Op::ShlA: {
            const bool carry = (a_ & 0x80u) != 0;
            a_ = static_cast<std::uint8_t>(a_ << 1);
            set_alu_flags(a_, carry, false);
            return step_ok(opcode, operand, cycles);
        }
        case Op::ShrA: {
            const bool carry = (a_ & 0x01u) != 0;
            a_ = static_cast<std::uint8_t>(a_ >> 1);
            set_alu_flags(a_, carry, false);
            return step_ok(opcode, operand, cycles);
        }
        case Op::SarA: {
            const bool carry = (a_ & 0x01u) != 0;
            a_ = static_cast<std::uint8_t>((a_ >> 1) | (a_ & 0x80u));
            set_alu_flags(a_, carry, false);
            return step_ok(opcode, operand, cycles);
        }
        case Op::Clc:
            flags_ = static_cast<std::uint8_t>(flags_ & ~FlagC);
            return step_ok(opcode, operand, cycles);
        case Op::Sec:
            flags_ = static_cast<std::uint8_t>(flags_ | FlagC);
            return step_ok(opcode, operand, cycles);
        case Op::Jmp:
            pc_ = operand;
            ++cycles;
            return step_ok(opcode, operand, cycles);
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
            if (condition(info->op)) {
                pc_ = operand;
                ++cycles;
            }
            return step_ok(opcode, operand, cycles);
        case Op::Call:
            if (!push(memory, next_pc)) {
                return step_fault(FaultKind::StackOverflow, instruction_pc,
                                  opcode, operand,
                                  "stack overflow at " +
                                      hex_byte(instruction_pc));
            }
            ++cycles;
            pc_ = operand;
            ++cycles;
            return step_ok(opcode, operand, cycles);
        case Op::OutA:
            if (out_) {
                out_(a_);
            }
            return step_ok(opcode, operand, cycles);
        case Op::InA: {
            const std::optional<std::uint8_t> value =
                in_ ? in_() : std::nullopt;
            if (value.has_value()) {
                a_ = *value;
                flags_ = static_cast<std::uint8_t>(flags_ & ~FlagZ);
            } else {
                a_ = 0;
                flags_ = static_cast<std::uint8_t>(flags_ | FlagZ);
            }
            return step_ok(opcode, operand, cycles);
        }
        default:
            return step_fault(FaultKind::IllegalOpcode, instruction_pc,
                              opcode, operand,
                              "unimplemented opcode " + hex_byte(opcode));
    }
}

}  // namespace kr8
