#include <array>
#include <cstdint>

#include "cpu_test_util.hpp"
#include "harness.hpp"

using namespace kr8;
using namespace kr8::test;

TEST(memory_starts_zeroed) {
    Memory memory;
    for (int address = 0; address < 256; ++address) {
        CHECK_EQ(memory.read8(static_cast<std::uint8_t>(address)), 0);
    }
}

TEST(memory_load_copies_and_zero_fills) {
    Memory memory;
    memory.write8(0x00, 0xAA);
    const std::array<std::uint8_t, 3> image{0x11, 0x22, 0x33};
    memory.load(image);
    CHECK_EQ(memory.read8(0x00), 0x11);
    CHECK_EQ(memory.read8(0x01), 0x22);
    CHECK_EQ(memory.read8(0x02), 0x33);
    CHECK_EQ(memory.read8(0x03), 0x00);
}

TEST(cpu_reset_state) {
    Cpu cpu;
    cpu.set_a(1);
    cpu.set_b(2);
    cpu.set_flags(FlagAll);
    cpu.reset(0x10, 0x20);
    CHECK_EQ(cpu.a(), 0);
    CHECK_EQ(cpu.b(), 0);
    CHECK_EQ(cpu.pc(), 0x10);
    CHECK_EQ(cpu.sp(), 0xFF);
    CHECK_EQ(cpu.flags(), 0);
    CHECK_EQ(cpu.stack_depth(), 0);
    CHECK_EQ(cpu.stack_floor(), 0x20);
    CHECK(!cpu.halted());
    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.instructions(), 0u);
    CHECK_EQ(cpu.cycles(), 0u);
}

TEST(cpu_fetches_one_byte_instruction) {
    Cpu cpu;
    Memory memory;
    const StepResult result = run_once(cpu, memory, {0x01});
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.pc(), 1);
    CHECK_EQ(result.cycles, 1u);
    CHECK_EQ(cpu.instructions(), 1u);
}

TEST(cpu_fetches_two_byte_instruction) {
    Cpu cpu;
    Memory memory;
    const StepResult result = run_once(cpu, memory, {0x0A, 0x42});
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.a(), 0x42);
    CHECK_EQ(cpu.pc(), 2);
    CHECK_EQ(result.cycles, 2u);
}

TEST(cpu_two_byte_instruction_at_fe_wraps_pc) {
    Cpu cpu;
    Memory memory;
    const StepResult result =
        run_once(cpu, memory, {0x0A, 0x42}, 0xFE);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.a(), 0x42);
    CHECK_EQ(cpu.pc(), 0);
}

TEST(cpu_two_byte_instruction_at_ff_faults) {
    Cpu cpu;
    Memory memory;
    const StepResult result = run_once(cpu, memory, {0x0A}, 0xFF);
    CHECK(result.status == StepStatus::Fault);
    CHECK(result.fault == FaultKind::PcWrap);
    CHECK_EQ(cpu.pc(), 0xFF);
    CHECK(cpu.faulted());
}

TEST(cpu_illegal_opcode_faults) {
    Cpu cpu;
    Memory memory;
    const StepResult result = run_once(cpu, memory, {0x0C});
    CHECK(result.status == StepStatus::Fault);
    CHECK(result.fault == FaultKind::IllegalOpcode);
    CHECK_EQ(cpu.pc(), 0);
    CHECK(!result.message.empty());
}

TEST(cpu_hlt_stops) {
    Cpu cpu;
    Memory memory;
    const StepResult result = run_once(cpu, memory, {0x00});
    CHECK(result.status == StepStatus::Halted);
    CHECK_EQ(result.pc, 0);
    CHECK_EQ(cpu.pc(), 0);
    CHECK(cpu.halted());
    CHECK(cpu.halt_kind() == HaltKind::Hlt);
    CHECK(!cpu.resume_from_break());
}

TEST(cpu_brk_stops_and_resumes) {
    Cpu cpu;
    Memory memory;
    const StepResult result = run_once(cpu, memory, {0x02, 0x01});
    CHECK(result.status == StepStatus::Break);
    CHECK_EQ(cpu.pc(), 0);
    CHECK(cpu.halted());
    CHECK(cpu.halt_kind() == HaltKind::Break);

    CHECK(cpu.resume_from_break());
    CHECK_EQ(cpu.pc(), 1);
    CHECK(!cpu.halted());

    const StepResult next = cpu.step(memory);
    CHECK(next.status == StepStatus::Ok);
    CHECK_EQ(cpu.pc(), 2);
}

TEST(cpu_step_after_halt_is_idempotent) {
    Cpu cpu;
    Memory memory;
    run_once(cpu, memory, {0x00});
    const std::uint64_t instructions = cpu.instructions();
    const StepResult again = cpu.step(memory);
    CHECK(again.status == StepStatus::Halted);
    CHECK_EQ(cpu.instructions(), instructions);
    CHECK_EQ(cpu.pc(), 0);
}
