#include <cstdint>
#include <initializer_list>

#include "cpu_test_util.hpp"
#include "harness.hpp"

using namespace kr8;
using namespace kr8::test;

namespace {

void load_program(Cpu& cpu, Memory& memory,
                  std::initializer_list<std::uint8_t> code,
                  std::uint8_t entry = 0,
                  std::uint16_t stack_floor = 0) {
    memory.reset();
    cpu.reset(entry, stack_floor);
    put(memory, entry, code);
}

void check_branch(std::uint8_t opcode, std::uint8_t flags, bool taken) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {opcode, 0x40});
    cpu.set_flags(flags);
    const StepResult result = cpu.step(memory);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.pc(), taken ? 0x40 : 2);
    CHECK_EQ(result.cycles, taken ? 3u : 2u);
}

}  // namespace

TEST(cpu_loads_and_stores_direct_and_indirect) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {
        0x0B, 0x20,  // MOV B, #0x20
        0x0A, 0x42,  // MOV A, #0x42
        0x13, 0x20,  // ST [0x20], A
        0x0A, 0x00,  // MOV A, #0
        0x10, 0x20,  // LD A, [0x20]
        0x00,        // HLT
    });
    for (int i = 0; i < 6; ++i) {
        const StepResult result = cpu.step(memory);
        CHECK(result.status == (i == 5 ? StepStatus::Halted
                                       : StepStatus::Ok));
    }
    CHECK_EQ(cpu.a(), 0x42);
    CHECK_EQ(memory.read8(0x20), 0x42);
    CHECK_EQ(cpu.pc(), 10);

    load_program(cpu, memory, {
        0x0B, 0x30,  // MOV B, #0x30
        0x0A, 0x55,  // MOV A, #0x55
        0x15,        // ST [B], A
        0x0A, 0x00,  // MOV A, #0
        0x12,        // LD A, [B]
        0x00,        // HLT
    });
    for (int i = 0; i < 6; ++i) {
        cpu.step(memory);
    }
    CHECK_EQ(cpu.a(), 0x55);
    CHECK_EQ(memory.read8(0x30), 0x55);
}

TEST(cpu_ld_and_st_b_direct) {
    Cpu cpu;
    Memory memory;
    memory.reset();
    cpu.reset(0, 0);
    memory.write8(0x40, 0x77);
    memory.write8(0, 0x11);  // LD B, [0x40]
    memory.write8(1, 0x40);
    cpu.step(memory);
    CHECK_EQ(cpu.b(), 0x77);

    memory.write8(0, 0x14);  // ST [0x40], B
    memory.write8(1, 0x40);
    cpu.set_pc(0);
    cpu.set_b(0x88);
    cpu.step(memory);
    CHECK_EQ(memory.read8(0x40), 0x88);
}

TEST(cpu_push_pop_a_and_b) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {
        0x0A, 0x12,  // MOV A, #0x12
        0x04,        // PUSH A
        0x0B, 0x34,  // MOV B, #0x34
        0x05,        // PUSH B
        0x0A, 0x00,  // MOV A, #0
        0x0B, 0x00,  // MOV B, #0
        0x06,        // POP A
        0x07,        // POP B
        0x00,        // HLT
    });
    for (int i = 0; i < 10; ++i) {
        cpu.step(memory);
    }
    CHECK_EQ(cpu.a(), 0x34);
    CHECK_EQ(cpu.b(), 0x12);
    CHECK_EQ(cpu.sp(), 0xFF);
    CHECK_EQ(cpu.stack_depth(), 0);
}

TEST(cpu_call_and_ret) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {
        0x5D, 0x06,  // CALL 0x06
        0x00,        // HLT
        0x01,        // NOP
        0x01,        // NOP
        0x01,        // NOP
        0x0A, 0x41,  // MOV A, #0x41
        0x03,        // RET
    });
    StepResult result = cpu.step(memory);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.pc(), 0x06);
    CHECK_EQ(cpu.sp(), 0xFE);
    CHECK_EQ(cpu.stack_depth(), 1);
    CHECK_EQ(memory.read8(0xFF), 0x02);

    result = cpu.step(memory);
    CHECK_EQ(cpu.a(), 0x41);
    CHECK_EQ(cpu.pc(), 0x08);

    result = cpu.step(memory);
    CHECK_EQ(cpu.pc(), 0x02);
    CHECK_EQ(cpu.sp(), 0xFF);
    CHECK_EQ(cpu.stack_depth(), 0);

    result = cpu.step(memory);
    CHECK(result.status == StepStatus::Halted);
    CHECK_EQ(cpu.pc(), 0x02);
}

TEST(cpu_nested_calls) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {
        0x5D, 0x06,  // 00: CALL 0x06
        0x00,        // 02: HLT
        0x01, 0x01, 0x01,  // 03: NOPs
        0x5D, 0x0A,  // 06: CALL 0x0A
        0x03,        // 08: RET
        0x01,        // 09: NOP
        0x0A, 0x55,  // 0A: MOV A, #0x55
        0x03,        // 0C: RET
    });
    for (int i = 0; i < 6; ++i) {
        cpu.step(memory);
    }
    CHECK_EQ(cpu.a(), 0x55);
    CHECK_EQ(cpu.pc(), 0x02);
    CHECK_EQ(cpu.stack_depth(), 0);
    CHECK_EQ(cpu.sp(), 0xFF);
}

TEST(cpu_call_at_fe_pushes_zero) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {0x5D, 0x00}, 0xFE);
    const StepResult result = cpu.step(memory);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.pc(), 0x00);
    CHECK_EQ(cpu.sp(), 0xFE);
    CHECK_EQ(cpu.stack_depth(), 1);
    CHECK_EQ(memory.read8(0xFF), 0x00);
}

TEST(cpu_stack_overflow_is_detected) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {0x04, 0x04}, 0, 0x10);
    cpu.set_sp(0x10);
    StepResult result = cpu.step(memory);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.sp(), 0x0F);
    result = cpu.step(memory);
    CHECK(result.status == StepStatus::Fault);
    CHECK(result.fault == FaultKind::StackOverflow);
}

TEST(cpu_stack_underflow_is_detected) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {0x06});
    const StepResult result = cpu.step(memory);
    CHECK(result.status == StepStatus::Fault);
    CHECK(result.fault == FaultKind::StackUnderflow);

    load_program(cpu, memory, {0x03});
    const StepResult ret = cpu.step(memory);
    CHECK(ret.status == StepStatus::Fault);
    CHECK(ret.fault == FaultKind::StackUnderflow);
}

TEST(cpu_branch_conditions) {
    check_branch(0x51, 0, false);                 // JZ
    check_branch(0x51, FlagZ, true);
    check_branch(0x52, 0, true);                  // JNZ
    check_branch(0x52, FlagZ, false);
    check_branch(0x53, FlagC, true);              // JC
    check_branch(0x53, 0, false);
    check_branch(0x54, 0, true);                  // JNC
    check_branch(0x54, FlagC, false);
    check_branch(0x55, FlagN, true);              // JN
    check_branch(0x55, 0, false);
    check_branch(0x56, 0, true);                  // JNN
    check_branch(0x56, FlagN, false);
    check_branch(0x57, FlagV, true);              // JV
    check_branch(0x57, 0, false);
    check_branch(0x58, 0, true);                  // JNV
    check_branch(0x58, FlagV, false);
}

TEST(cpu_signed_branch_conditions) {
    check_branch(0x59, FlagN, true);              // JLT N != V
    check_branch(0x59, FlagV, true);
    check_branch(0x59, 0, false);
    check_branch(0x59, FlagN | FlagV, false);

    check_branch(0x5A, 0, true);                  // JGE N == V
    check_branch(0x5A, FlagN | FlagV, true);
    check_branch(0x5A, FlagN, false);
    check_branch(0x5A, FlagV, false);

    check_branch(0x5B, 0, true);                  // JGT !Z && N == V
    check_branch(0x5B, FlagZ, false);
    check_branch(0x5B, FlagN, false);
    check_branch(0x5B, FlagN | FlagV, true);

    check_branch(0x5C, FlagZ, true);              // JLE Z || N != V
    check_branch(0x5C, FlagN, true);
    check_branch(0x5C, 0, false);
    check_branch(0x5C, FlagN | FlagV, false);
}

TEST(cpu_jmp_is_always_taken) {
    Cpu cpu;
    Memory memory;
    load_program(cpu, memory, {0x50, 0x40});
    const StepResult result = cpu.step(memory);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.pc(), 0x40);
    CHECK_EQ(result.cycles, 3u);
}

TEST(cpu_store_and_push_do_not_change_flags) {
    Cpu cpu;
    Memory memory;
    memory.reset();
    cpu.reset(0, 0);
    memory.write8(0, 0x13);  // ST [0x20], A
    memory.write8(1, 0x20);
    cpu.set_flags(FlagAll);
    cpu.step(memory);
    CHECK_EQ(cpu.flags() & FlagAll, FlagAll);

    load_program(cpu, memory, {0x04});  // PUSH A
    cpu.set_flags(FlagAll);
    cpu.step(memory);
    CHECK_EQ(cpu.flags() & FlagAll, FlagAll);
}
