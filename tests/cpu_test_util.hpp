#pragma once

#include <cstdint>
#include <initializer_list>

#include "kr8/cpu.hpp"

namespace kr8::test {

inline void put(Memory& memory, std::uint16_t address,
                std::initializer_list<std::uint8_t> bytes) {
    for (const std::uint8_t byte : bytes) {
        memory.write8(static_cast<std::uint8_t>(address), byte);
        ++address;
    }
}

inline StepResult run_once(Cpu& cpu, Memory& memory,
                           std::initializer_list<std::uint8_t> code,
                           std::uint8_t entry = 0,
                           std::uint16_t stack_floor = 0) {
    memory.reset();
    cpu.reset(entry, stack_floor);
    put(memory, entry, code);
    return cpu.step(memory);
}

}  // namespace kr8::test
