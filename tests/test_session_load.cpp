#include <string>

#include "harness.hpp"
#include "kr8/session.hpp"

using namespace kr8;

namespace {

const char* kProgram =
    "start:\n"
    "  MOV A, #0x41\n"
    "  OUT A\n"
    "  HLT\n";

}  // namespace

TEST(session_exposes_assembly_listing) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kProgram, "test.asm", error));
    CHECK(!session.listing().empty());
    CHECK(session.listing().find("ADDR") != std::string::npos);
}

TEST(session_loads_source_and_snapshot) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kProgram, "test.asm", error));
    CHECK(session.loaded());
    if (!session.loaded()) {
        return;
    }
    const Snapshot snapshot = session.snapshot();
    CHECK(snapshot.loaded);
    CHECK(!snapshot.running);
    CHECK_EQ(snapshot.pc, 0);
    CHECK_EQ(snapshot.a, 0);
    CHECK_EQ(snapshot.sp, 0xFF);
    CHECK_EQ(snapshot.line, 2);
    CHECK_EQ(snapshot.function, std::string("start"));
    CHECK_EQ(snapshot.message, std::string("loaded"));
    CHECK_EQ(snapshot.frames.size(), 1u);
    CHECK_EQ(session.disassembly().size(), 3u);
    CHECK_EQ(session.source_lines().size(), 4u);
    CHECK(session.executable_lines().count(2) == 1u);
    CHECK(session.executable_lines().count(3) == 1u);
    CHECK(session.executable_lines().count(4) == 1u);

    const Symbol* start = session.symbol_at(0);
    CHECK(start != nullptr);
    if (start != nullptr) {
        CHECK_EQ(start->name, std::string("start"));
    }
    CHECK_EQ(session.line_at(0), 2);
    CHECK_EQ(session.function_at(0), std::string("start"));
}

TEST(session_restart_resets_state_and_keeps_breakpoints) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kProgram, "test.asm", error));
    CHECK(session.step_instruction());
    session.toggle_address_breakpoint(0);
    CHECK(session.has_breakpoint(0));

    CHECK(session.restart(error));
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.pc, 0);
    CHECK_EQ(snapshot.a, 0);
    CHECK_EQ(snapshot.instructions, 0u);
    CHECK_EQ(session.console(), std::string());
    CHECK(session.has_breakpoint(0));
}

TEST(session_failed_assemble_keeps_previous_image) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kProgram, "test.asm", error));
    CHECK(session.step_instruction());
    const std::uint8_t pc = session.snapshot().pc;
    CHECK(!session.assemble_source("this is not assembly\n", "bad.asm",
                                   error));
    CHECK(!error.empty());
    CHECK(session.loaded());
    CHECK_EQ(session.snapshot().pc, pc);
}

TEST(session_failed_load_keeps_previous_image) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kProgram, "test.asm", error));
    CHECK(!session.load_image("definitely-not-here.k8", error));
    CHECK(!error.empty());
    CHECK(session.loaded());
}

TEST(session_unload_clears_state) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kProgram, "test.asm", error));
    session.unload();
    CHECK(!session.loaded());
    CHECK(session.source_lines().empty());
    CHECK(session.disassembly().empty());
    CHECK_EQ(session.snapshot().frames.size(), 0u);
}
