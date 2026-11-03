#include <cstdint>

#include "cpu_test_util.hpp"
#include "harness.hpp"

using namespace kr8;
using namespace kr8::test;

namespace {

StepResult exec_alu(Cpu& cpu, Memory& memory, std::uint8_t opcode,
                    std::uint8_t a, std::uint8_t b,
                    std::uint8_t flags = 0) {
    memory.reset();
    cpu.reset(0, 0);
    memory.write8(0, opcode);
    cpu.set_a(a);
    cpu.set_b(b);
    cpu.set_flags(flags);
    return cpu.step(memory);
}

StepResult exec_imm(Cpu& cpu, Memory& memory, std::uint8_t opcode,
                    std::uint8_t a, std::uint8_t immediate,
                    std::uint8_t flags = 0) {
    memory.reset();
    cpu.reset(0, 0);
    memory.write8(0, opcode);
    memory.write8(1, immediate);
    cpu.set_a(a);
    cpu.set_flags(flags);
    return cpu.step(memory);
}

void check_alu(std::uint8_t opcode, std::uint8_t a, std::uint8_t b,
               std::uint8_t expected_a, std::uint8_t expected_flags) {
    Cpu cpu;
    Memory memory;
    const StepResult result = exec_alu(cpu, memory, opcode, a, b);
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(cpu.a(), expected_a);
    CHECK_EQ(cpu.flags() & FlagAll, expected_flags);
}

}  // namespace

TEST(cpu_add_sets_result_and_flags) {
    check_alu(0x20, 0x0F, 0x01, 0x10, 0);
    check_alu(0x20, 0x7F, 0x01, 0x80, FlagN | FlagV);
    check_alu(0x20, 0xFF, 0x01, 0x00, FlagC | FlagZ);
    check_alu(0x20, 0x80, 0x80, 0x00, FlagC | FlagZ | FlagV);
    check_alu(0x20, 0x50, 0x50, 0xA0, FlagN | FlagV);
    check_alu(0x20, 0xFF, 0xFF, 0xFE, FlagC | FlagN);
}

TEST(cpu_adc_includes_carry) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x21, 0xFF, 0x00, FlagC);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x21, 0x7F, 0x00, FlagC);
    CHECK_EQ(cpu.a(), 0x80);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN | FlagV);

    exec_alu(cpu, memory, 0x21, 0x00, 0x00, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagZ);
}

TEST(cpu_sub_sets_borrow_and_overflow) {
    check_alu(0x22, 5, 3, 2, FlagC);
    check_alu(0x22, 3, 5, 0xFE, FlagN);
    check_alu(0x22, 0, 1, 0xFF, FlagN);
    check_alu(0x22, 0x80, 1, 0x7F, FlagC | FlagV);
    check_alu(0x22, 0x80, 0x80, 0x00, FlagC | FlagZ);
    check_alu(0x22, 0x00, 0x80, 0x80, FlagN | FlagV);
}

TEST(cpu_sbc_includes_borrow) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x23, 5, 3, FlagC);
    CHECK_EQ(cpu.a(), 2);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC);

    exec_alu(cpu, memory, 0x23, 5, 3, 0);
    CHECK_EQ(cpu.a(), 1);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC);

    exec_alu(cpu, memory, 0x23, 0, 0, 0);
    CHECK_EQ(cpu.a(), 0xFF);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);
}

TEST(cpu_cmp_does_not_write_a) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x24, 5, 5);
    CHECK_EQ(cpu.a(), 5);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x24, 3, 5);
    CHECK_EQ(cpu.a(), 3);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);
}

TEST(cpu_logic_flags_clear_carry_and_overflow) {
    check_alu(0x25, 0xF0, 0x0F, 0x00, FlagZ);
    check_alu(0x26, 0x80, 0x00, 0x80, FlagN);
    check_alu(0x27, 0xFF, 0x0F, 0xF0, FlagN);
    check_alu(0x28, 0x80, 0x80, 0x80, FlagN);
}

TEST(cpu_not_and_neg) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x40, 0x0F, 0);
    CHECK_EQ(cpu.a(), 0xF0);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);

    exec_alu(cpu, memory, 0x40, 0xFF, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagZ);

    exec_alu(cpu, memory, 0x41, 0x00, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x41, 0x01, 0);
    CHECK_EQ(cpu.a(), 0xFF);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);

    exec_alu(cpu, memory, 0x41, 0x80, 0);
    CHECK_EQ(cpu.a(), 0x80);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN | FlagV);
}

TEST(cpu_inc_and_dec) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x42, 0xFF, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x42, 0x7F, 0);
    CHECK_EQ(cpu.a(), 0x80);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN | FlagV);

    exec_alu(cpu, memory, 0x43, 0x00, 0);
    CHECK_EQ(cpu.a(), 0xFF);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);

    exec_alu(cpu, memory, 0x43, 0x80, 0);
    CHECK_EQ(cpu.a(), 0x7F);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagV);

    exec_alu(cpu, memory, 0x44, 0, 0xFF);
    CHECK_EQ(cpu.b(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x45, 0, 0x00);
    CHECK_EQ(cpu.b(), 0xFF);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);
}

TEST(cpu_shifts) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x46, 0x80, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x46, 0x01, 0);
    CHECK_EQ(cpu.a(), 0x02);
    CHECK_EQ(cpu.flags() & FlagAll, 0);

    exec_alu(cpu, memory, 0x46, 0xFF, 0);
    CHECK_EQ(cpu.a(), 0xFE);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagN);

    exec_alu(cpu, memory, 0x47, 0x01, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_alu(cpu, memory, 0x47, 0x80, 0);
    CHECK_EQ(cpu.a(), 0x40);
    CHECK_EQ(cpu.flags() & FlagAll, 0);

    exec_alu(cpu, memory, 0x48, 0x80, 0);
    CHECK_EQ(cpu.a(), 0xC0);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);

    exec_alu(cpu, memory, 0x48, 0x81, 0);
    CHECK_EQ(cpu.a(), 0xC0);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagN);

    exec_alu(cpu, memory, 0x48, 0x01, 0);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);
}

TEST(cpu_carry_control_preserves_other_flags) {
    Cpu cpu;
    Memory memory;
    exec_alu(cpu, memory, 0x49, 0, 0, FlagZ | FlagN | FlagC);
    CHECK_EQ(cpu.flags() & FlagAll, FlagZ | FlagN);

    exec_alu(cpu, memory, 0x4A, 0, 0, FlagZ | FlagN);
    CHECK_EQ(cpu.flags() & FlagAll, FlagZ | FlagN | FlagC);
}

TEST(cpu_immediate_alu_forms) {
    Cpu cpu;
    Memory memory;
    exec_imm(cpu, memory, 0x30, 0x0F, 0x01);
    CHECK_EQ(cpu.a(), 0x10);
    CHECK_EQ(cpu.flags() & FlagAll, 0);

    exec_imm(cpu, memory, 0x34, 5, 5);
    CHECK_EQ(cpu.a(), 5);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagZ);

    exec_imm(cpu, memory, 0x35, 0xF0, 0x0F);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagZ);

    exec_imm(cpu, memory, 0x36, 0x80, 0x00);
    CHECK_EQ(cpu.a(), 0x80);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);

    exec_imm(cpu, memory, 0x37, 0xFF, 0x0F);
    CHECK_EQ(cpu.a(), 0xF0);
    CHECK_EQ(cpu.flags() & FlagAll, FlagN);
}

TEST(cpu_mov_sets_zn_and_preserves_carry_overflow) {
    Cpu cpu;
    Memory memory;
    memory.reset();
    cpu.reset(0, 0);
    memory.write8(0, 0x0A);
    memory.write8(1, 0x00);
    cpu.set_flags(FlagC | FlagV | FlagN);
    cpu.step(memory);
    CHECK_EQ(cpu.a(), 0x00);
    CHECK_EQ(cpu.flags() & FlagAll, FlagC | FlagV | FlagZ);
}
