#include <cstdint>
#include <string>

#include "harness.hpp"
#include "kr8/session.hpp"

using namespace kr8;

namespace {

const char* kEcho =
    "loop:\n"
    "  IN A\n"
    "  OUT A\n"
    "  HLT\n";

}  // namespace

TEST(session_console_and_input) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kEcho, "echo.asm", error));
    session.push_input(static_cast<std::uint8_t>('x'));
    CHECK_EQ(session.input_size(), 1u);
    CHECK(session.step_instruction());
    CHECK(session.step_instruction());
    CHECK(session.step_instruction());
    CHECK_EQ(session.console(), std::string("x"));
    CHECK_EQ(session.input_size(), 0u);
}

TEST(session_stats_and_reset) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kEcho, "echo.asm", error));
    session.push_input(static_cast<std::uint8_t>('x'));
    for (int i = 0; i < 3; ++i) {
        session.step_instruction();
    }
    CHECK_EQ(session.instructions_executed(), 3u);
    CHECK(session.cycles() > 0);
    const std::uint8_t pc = session.snapshot().pc;
    session.reset_stats();
    CHECK_EQ(session.instructions_executed(), 0u);
    CHECK_EQ(session.cycles(), 0u);
    CHECK_EQ(session.snapshot().pc, pc);
}

TEST(session_register_and_flags_editing) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kEcho, "echo.asm", error));
    session.set_register(0, 0x41);
    session.set_register(1, 0x42);
    Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.a, 0x41);
    CHECK_EQ(snapshot.b, 0x42);

    session.set_flag(0, true);  // Z
    session.set_flag(2, true);  // N
    snapshot = session.snapshot();
    CHECK((snapshot.flags & FlagZ) != 0);
    CHECK((snapshot.flags & FlagN) != 0);
    CHECK((snapshot.flags & FlagC) == 0);

    session.set_register(2, 4);  // PC
    CHECK_EQ(session.snapshot().pc, 4);
    CHECK(session.snapshot().message.find("PC edited") != std::string::npos);
}

TEST(session_sp_edit_invalidates_call_stack) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kEcho, "echo.asm", error));
    session.set_register(3, 0xFE);
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.sp, 0xFE);
    CHECK_EQ(snapshot.frames.size(), 1u);
    CHECK(snapshot.message.find("call stack unavailable") != std::string::npos);
}

TEST(session_memory_editing) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kEcho, "echo.asm", error));
    session.write8(0x20, 0x77);
    CHECK_EQ(session.read8(0x20), 0x77);

}

TEST(session_input_queue_can_be_cleared) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kEcho, "echo.asm", error));
    session.push_input(0x41);
    session.push_input(0x42);
    CHECK_EQ(session.input_size(), 2u);
    session.clear_input();
    CHECK_EQ(session.input_size(), 0u);
}
