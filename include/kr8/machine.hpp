#pragma once

#include <cstdint>
#include <deque>
#include <span>
#include <string>

#include "kr8/cpu.hpp"

namespace kr8 {

/// Owns the CPU, RAM, console output, and input queue for one loaded image.
class Machine {
public:
    Machine();
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;
    Machine(Machine&&) = delete;
    Machine& operator=(Machine&&) = delete;

    void load(std::span<const std::uint8_t> image, std::uint8_t entry = 0,
              std::uint16_t stack_floor = 0);
    void reset();

    StepResult step();

    Memory& memory() noexcept { return memory_; }
    const Memory& memory() const noexcept { return memory_; }
    Cpu& cpu() noexcept { return cpu_; }
    const Cpu& cpu() const noexcept { return cpu_; }

    bool halted() const noexcept { return cpu_.halted(); }
    bool faulted() const noexcept { return cpu_.faulted(); }
    FaultKind fault() const noexcept { return cpu_.fault(); }
    const std::string& fault_message() const noexcept {
        return cpu_.fault_message();
    }

    const std::string& console_output() const noexcept { return console_; }
    void clear_console() { console_.clear(); }

    void push_input(std::uint8_t value) { input_.push_back(value); }
    void clear_input() { input_.clear(); }
    std::size_t input_size() const noexcept { return input_.size(); }

    std::uint8_t entry() const noexcept { return entry_; }
    std::uint16_t stack_floor() const noexcept { return stack_floor_; }

    std::uint64_t instructions() const noexcept {
        return cpu_.instructions();
    }
    std::uint64_t cycles() const noexcept { return cpu_.cycles(); }

private:
    Memory memory_;
    Cpu cpu_;
    std::string console_;
    std::deque<std::uint8_t> input_;
    std::uint8_t entry_ = 0;
    std::uint16_t stack_floor_ = 0;
};

}  // namespace kr8
