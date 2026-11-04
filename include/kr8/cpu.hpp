#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

#include "kr8/isa.hpp"

namespace kr8 {

/// 256-byte RAM shared by code, data, and stack.
class Memory {
public:
    Memory();

    void reset();
    void load(std::span<const std::uint8_t> image);

    std::uint8_t read8(std::uint8_t address) const noexcept {
        return bytes_[address];
    }
    void write8(std::uint8_t address, std::uint8_t value) noexcept {
        bytes_[address] = value;
    }

    const std::array<std::uint8_t, 256>& bytes() const noexcept {
        return bytes_;
    }
    std::array<std::uint8_t, 256>& bytes() noexcept { return bytes_; }

private:
    std::array<std::uint8_t, 256> bytes_{};
};

enum class StepStatus : std::uint8_t {
    Ok,
    Halted,
    Break,
    Fault,
};

enum class FaultKind : std::uint8_t {
    None,
    IllegalOpcode,
    StackOverflow,
    StackUnderflow,
    PcWrap,
};

enum class HaltKind : std::uint8_t {
    None,
    Hlt,
    Break,
};

struct StepResult {
    StepStatus status = StepStatus::Ok;
    FaultKind fault = FaultKind::None;
    std::uint8_t pc = 0;
    std::uint32_t cycles = 0;
    std::uint8_t opcode = 0;
    std::uint8_t operand = 0;
    std::string message;
};

class Cpu {
public:
    using OutHandler = std::function<void(std::uint8_t)>;
    using InHandler = std::function<std::optional<std::uint8_t>()>;

    Cpu();

    void reset(std::uint8_t entry = 0, std::uint16_t stack_floor = 0);

    /// Execute one instruction. If the CPU is halted or faulted, returns the
    /// existing stop state without executing anything.
    StepResult step(Memory& memory);

    /// Continue past a BRK stop. Returns false for HLT or a fault.
    bool resume_from_break();

    std::uint8_t a() const noexcept { return a_; }
    std::uint8_t b() const noexcept { return b_; }
    std::uint8_t pc() const noexcept { return pc_; }
    std::uint8_t sp() const noexcept { return sp_; }
    std::uint8_t flags() const noexcept { return flags_; }

    void set_a(std::uint8_t value) noexcept { a_ = value; }
    void set_b(std::uint8_t value) noexcept { b_ = value; }
    void set_pc(std::uint8_t value) noexcept { pc_ = value; }
    void set_sp(std::uint8_t value) noexcept { sp_ = value; }
    void set_flags(std::uint8_t value) noexcept {
        flags_ = static_cast<std::uint8_t>(value & FlagAll);
    }

    bool flag(std::uint8_t mask) const noexcept {
        return (flags_ & mask) != 0;
    }
    void set_flag(std::uint8_t mask, bool value) noexcept {
        if (value) {
            flags_ = static_cast<std::uint8_t>(flags_ | mask);
        } else {
            flags_ = static_cast<std::uint8_t>(flags_ & ~mask);
        }
    }

    std::uint16_t stack_depth() const noexcept { return stack_depth_; }
    std::uint16_t stack_floor() const noexcept { return stack_floor_; }

    bool halted() const noexcept { return halted_; }
    HaltKind halt_kind() const noexcept { return halt_kind_; }
    bool faulted() const noexcept { return fault_ != FaultKind::None; }
    FaultKind fault() const noexcept { return fault_; }
    const std::string& fault_message() const noexcept {
        return fault_message_;
    }

    std::uint64_t instructions() const noexcept { return instructions_; }
    std::uint64_t cycles() const noexcept { return cycles_; }
    void reset_counters() noexcept {
        instructions_ = 0;
        cycles_ = 0;
    }
    std::uint8_t last_opcode() const noexcept { return last_opcode_; }
    std::uint8_t last_operand() const noexcept { return last_operand_; }

    void set_out_handler(OutHandler handler) { out_ = std::move(handler); }
    void set_in_handler(InHandler handler) { in_ = std::move(handler); }

private:
    StepResult current_stop() const;
    StepResult step_ok(std::uint8_t opcode, std::uint8_t operand,
                       std::uint32_t cycles);
    StepResult step_halt(std::uint8_t instruction_pc, std::uint8_t opcode,
                         std::uint8_t operand, std::uint32_t cycles);
    StepResult step_break(std::uint8_t instruction_pc, std::uint8_t opcode,
                          std::uint32_t cycles);
    StepResult step_fault(FaultKind kind, std::uint8_t instruction_pc,
                          std::uint8_t opcode, std::uint8_t operand,
                          std::string message);

    bool push(Memory& memory, std::uint8_t value);
    bool pop(Memory& memory, std::uint8_t& value);
    void set_zn(std::uint8_t value);
    void set_alu_flags(std::uint8_t value, bool carry, bool overflow);
    bool condition(Op op) const;

    std::uint8_t a_ = 0;
    std::uint8_t b_ = 0;
    std::uint8_t pc_ = 0;
    std::uint8_t sp_ = 0xFF;
    std::uint8_t flags_ = 0;

    std::uint16_t stack_depth_ = 0;
    std::uint16_t stack_floor_ = 0;

    bool halted_ = false;
    HaltKind halt_kind_ = HaltKind::None;
    FaultKind fault_ = FaultKind::None;
    std::string fault_message_;

    std::uint64_t instructions_ = 0;
    std::uint64_t cycles_ = 0;
    std::uint8_t last_opcode_ = 0;
    std::uint8_t last_operand_ = 0;

    OutHandler out_;
    InHandler in_;
};

}  // namespace kr8
