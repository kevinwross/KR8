#include <string>

#include "harness.hpp"
#include "kr8/session.hpp"

using namespace kr8;

namespace {

const char* kStraight =
    "start:\n"
    "  MOV A, #1\n"
    "  MOV B, #2\n"
    "  MOV A, #3\n"
    "  HLT\n";

}  // namespace

TEST(session_address_breakpoint_stops_and_continues) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kStraight, "straight.asm", error));
    session.toggle_address_breakpoint(2);
    CHECK(session.has_breakpoint(2));

    const RunStop stop = session.run_slice(100);
    CHECK(stop == RunStop::Breakpoint);
    CHECK(!session.running());
    CHECK_EQ(session.snapshot().pc, 2);
    CHECK(session.snapshot().message.find("breakpoint") != std::string::npos);

    const RunStop next = session.run_slice(100);
    CHECK(next == RunStop::Halted);
    CHECK_EQ(session.snapshot().pc, 6);
    CHECK_EQ(session.snapshot().a, 3);
    CHECK_EQ(session.snapshot().b, 2);
}

TEST(session_line_breakpoint_stops_on_the_line) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kStraight, "straight.asm", error));
    session.toggle_line_breakpoint(3);
    CHECK(session.has_line_breakpoint(3));
    const RunStop stop = session.run_slice(100);
    CHECK(stop == RunStop::Breakpoint);
    CHECK_EQ(session.snapshot().line, 3);
}

TEST(session_line_breakpoint_snaps_to_next_executable_line) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(
        "; comment\n"
        "start:\n"
        "  MOV A, #1\n"
        "; another comment\n"
        "  MOV B, #2\n"
        "  HLT\n",
        "snap.asm", error));
    session.toggle_line_breakpoint(4);
    CHECK(session.has_line_breakpoint(5));
    CHECK(!session.has_line_breakpoint(4));
    CHECK(session.snapshot().message.find("moved") != std::string::npos);
}

TEST(session_symbol_breakpoint) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kStraight, "straight.asm", error));
    session.toggle_symbol_breakpoint("start");
    CHECK(session.has_breakpoint(0));
}

TEST(session_breakpoints_survive_restart) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kStraight, "straight.asm", error));
    session.toggle_address_breakpoint(2);
    session.toggle_line_breakpoint(4);
    CHECK(session.restart(error));
    CHECK(session.has_breakpoint(2));
    CHECK(session.has_line_breakpoint(4));
}

TEST(session_run_slice_budget_keeps_running) {
    Session session;
    std::string error;
    CHECK(session.assemble_source("loop:\n  JMP loop\n", "loop.asm", error));
    const RunStop stop = session.run_slice(10);
    CHECK(stop == RunStop::Budget);
    CHECK(session.running());
    CHECK_EQ(session.instructions_executed(), 10u);
    session.stop_run("paused");
    CHECK(!session.running());
}

TEST(session_run_slice_without_image) {
    Session session;
    const RunStop stop = session.run_slice(10);
    CHECK(stop == RunStop::NotLoaded);
}

TEST(session_step_from_breakpoint_executes_instruction) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kStraight, "straight.asm", error));
    session.toggle_address_breakpoint(2);
    CHECK(session.run_slice(100) == RunStop::Breakpoint);
    CHECK_EQ(session.snapshot().pc, 2);
    CHECK(session.step_instruction());
    CHECK_EQ(session.snapshot().pc, 4);
}

TEST(session_run_slice_reports_break_and_fault) {
    Session session;
    std::string error;
    CHECK(session.assemble_source("start:\n  BRK\n  HLT\n", "brk.asm",
                                  error));
    CHECK(session.run_slice(10) == RunStop::Break);
    CHECK(session.snapshot().message.find("BRK") != std::string::npos);

    Image image;
    image.ram[0] = 0x0C;
    image.mark_code(0);
    image.entry = 0;
    image.image_high = 1;
    Session faulty;
    CHECK(faulty.load_image(image, "fault", error));
    CHECK(faulty.run_slice(10) == RunStop::Fault);
    CHECK(faulty.snapshot().message.find("illegal") != std::string::npos);
}
