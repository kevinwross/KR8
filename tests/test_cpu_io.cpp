#include <cstdint>
#include <optional>
#include <vector>

#include "cpu_test_util.hpp"
#include "harness.hpp"

using namespace kr8;
using namespace kr8::test;

namespace {

StepResult run_with_flags(Cpu& cpu, Memory& memory,
                          std::initializer_list<std::uint8_t> code,
                          std::uint8_t flags) {
    memory.reset();
    cpu.reset(0, 0);
    put(memory, 0, code);
    cpu.set_flags(flags);
    return cpu.step(memory);
}

}  // namespace

TEST(cpu_out_sends_a_to_the_handler) {
    Cpu cpu;
    Memory memory;
    std::vector<std::uint8_t> output;
    cpu.set_out_handler(
        [&output](std::uint8_t value) { output.push_back(value); });

    memory.reset();
    cpu.reset(0, 0);
    put(memory, 0, {0x0A, 0x41, 0x60, 0x00});  // MOV A,#0x41; OUT A; HLT
    cpu.step(memory);
    cpu.step(memory);
    CHECK_EQ(output.size(), 1u);
    CHECK_EQ(output[0], 0x41);
}

TEST(cpu_in_reads_a_byte_and_clears_z) {
    Cpu cpu;
    Memory memory;
    cpu.set_in_handler([]() { return std::optional<std::uint8_t>{std::uint8_t{0x41}}; });
    const StepResult result =
        run_with_flags(cpu, memory, {0x61}, FlagC | FlagN | FlagV);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.a(), 0x41);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagN | FlagV);
}

TEST(cpu_in_with_zero_byte_is_a_real_byte) {
    Cpu cpu;
    Memory memory;
    cpu.set_in_handler([]() { return std::optional<std::uint8_t>{std::uint8_t{0x00}}; });
    run_with_flags(cpu, memory, {0x61}, FlagC | FlagN | FlagV);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagN | FlagV);
}

TEST(cpu_in_with_no_byte_sets_z_and_preserves_other_flags) {
    Cpu cpu;
    Memory memory;
    cpu.set_in_handler([]() { return std::optional<std::uint8_t>(); });
    run_with_flags(cpu, memory, {0x61}, FlagC | FlagN | FlagV);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagZ | FlagC | FlagN | FlagV);
}

TEST(cpu_in_without_handler_returns_no_byte) {
    Cpu cpu;
    Memory memory;
    run_once(cpu, memory, {0x61});
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagZ, FlagZ);
}

TEST(cpu_cycle_model) {
    Cpu cpu;
    Memory memory;

    StepResult result = run_once(cpu, memory, {0x20});  // ADD A,B
    CHECK_EQ(result.cycles, 1u);

    result = run_once(cpu, memory, {0x0A, 0x01});  // MOV A,#
    CHECK_EQ(result.cycles, 2u);

    memory.reset();
    cpu.reset(0, 0);
    memory.write8(0, 0x12);  // LD A,[B]
    cpu.set_b(0x20);
    result = cpu.step(memory);
    CHECK_EQ(result.cycles, 2u);

    result = run_once(cpu, memory, {0x10, 0x20});  // LD A,[0x20]
    CHECK_EQ(result.cycles, 3u);

    result = run_once(cpu, memory, {0x13, 0x20});  // ST [0x20],A
    CHECK_EQ(result.cycles, 3u);

    result = run_once(cpu, memory, {0x51, 0x40});  // JZ not taken
    CHECK_EQ(result.cycles, 2u);

    result = run_with_flags(cpu, memory, {0x51, 0x40}, FlagZ);  // JZ taken
    CHECK_EQ(result.cycles, 3u);

    result = run_once(cpu, memory, {0x5D, 0x06});  // CALL
    CHECK_EQ(result.cycles, 4u);

    result = run_once(cpu, memory, {0x04, 0x03});  // PUSH; RET
    CHECK_EQ(result.cycles, 2u);
    result = cpu.step(memory);
    CHECK_EQ(result.cycles, 2u);

    result = run_once(cpu, memory, {0x04, 0x06});  // PUSH; POP
    CHECK_EQ(result.cycles, 2u);
    result = cpu.step(memory);
    CHECK_EQ(result.cycles, 2u);

    result = run_once(cpu, memory, {0x60});  // OUT A
    CHECK_EQ(result.cycles, 1u);

    result = run_once(cpu, memory, {0x61});  // IN A
    CHECK_EQ(result.cycles, 1u);
}
