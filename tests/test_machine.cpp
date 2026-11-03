#include <array>
#include <cstdint>
#include <string>

#include "harness.hpp"
#include "kr8/machine.hpp"

using namespace kr8;

namespace {

std::array<std::uint8_t, 256> image_with(
    std::initializer_list<std::uint8_t> code) {
    std::array<std::uint8_t, 256> image{};
    std::size_t index = 0;
    for (const std::uint8_t byte : code) {
        image[index++] = byte;
    }
    return image;
}

}  // namespace

TEST(machine_runs_a_program_and_captures_output) {
    Machine machine;
    machine.load(image_with({0x0A, 0x41, 0x60, 0x00}), 0, 4);
    CHECK_EQ(machine.entry(), 0);
    CHECK_EQ(machine.stack_floor(), 4);

    StepResult result = machine.step();
    CHECK(result.status == StepStatus::Ok);
    result = machine.step();
    CHECK(result.status == StepStatus::Ok);
    result = machine.step();
    CHECK(result.status == StepStatus::Halted);

    CHECK_EQ(machine.console_output(), std::string("A"));
    CHECK_EQ(machine.cpu().a(), 0x41);
    CHECK(machine.halted());
    CHECK_EQ(machine.instructions(), 3u);
    CHECK_EQ(machine.cycles(), 4u);
}

TEST(machine_reads_queued_input) {
    Machine machine;
    machine.load(image_with({0x61, 0x60, 0x00}));
    machine.push_input(static_cast<std::uint8_t>('x'));
    CHECK_EQ(machine.input_size(), 1u);

    StepResult result = machine.step();
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(machine.cpu().a(), static_cast<std::uint8_t>('x'));
    CHECK_EQ(machine.cpu().flags() & FlagZ, 0);

    result = machine.step();
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(machine.console_output(), std::string("x"));
    CHECK_EQ(machine.input_size(), 0u);
}

TEST(machine_in_with_empty_queue_sets_z) {
    Machine machine;
    machine.load(image_with({0x61, 0x00}));
    const StepResult result = machine.step();
    CHECK(result.status == StepStatus::Ok);
    CHECK_EQ(machine.cpu().a(), 0);
    CHECK_EQ(machine.cpu().flags() & FlagZ, FlagZ);
}

TEST(machine_reset_clears_console_and_input) {
    Machine machine;
    machine.load(image_with({0x00}));
    machine.push_input(0x41);
    machine.step();
    machine.reset();
    CHECK(machine.console_output().empty());
    CHECK_EQ(machine.input_size(), 0u);
    CHECK_EQ(machine.cpu().pc(), 0);
    CHECK_EQ(machine.cpu().a(), 0);
    CHECK(!machine.halted());
}

TEST(machine_propagates_faults) {
    Machine machine;
    machine.load(image_with({0x0C}));
    const StepResult result = machine.step();
    CHECK(result.status == StepStatus::Fault);
    CHECK(machine.faulted());
    CHECK(machine.fault() == FaultKind::IllegalOpcode);
    CHECK(!machine.fault_message().empty());
}

TEST(machine_writes_memory) {
    Machine machine;
    machine.load(image_with({0x0A, 0x42, 0x13, 0x20, 0x00}));
    for (int i = 0; i < 3; ++i) {
        machine.step();
    }
    CHECK_EQ(machine.memory().read8(0x20), 0x42);
}
