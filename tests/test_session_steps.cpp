#include <string>

#include "harness.hpp"
#include "kr8/session.hpp"

using namespace kr8;

namespace {

const char* kCalls =
    "start:\n"
    "  MOV B, #msg\n"
    "  CALL puts\n"
    "  OUT A\n"
    "  HLT\n"
    "puts:\n"
    "  LD A, [B]\n"
    "  JZ puts_done\n"
    "  OUT A\n"
    "  INC B\n"
    "  JMP puts\n"
    "puts_done:\n"
    "  RET\n"
    "msg: .asciz \"Hi\"\n";

}  // namespace

TEST(session_step_instruction_and_frames) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction());
    CHECK_EQ(session.snapshot().pc, 2);
    CHECK(session.step_instruction());  // CALL puts
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.pc, 6);
    CHECK_EQ(snapshot.frames.size(), 2u);
    CHECK_EQ(snapshot.frames[0].function, std::string("puts"));
    CHECK_EQ(snapshot.frames[0].line, 7);
    CHECK_EQ(snapshot.frames[1].function, std::string("start"));
    CHECK_EQ(snapshot.frames[1].line, 3);
    CHECK_EQ(snapshot.frames[1].pc, 2);
    CHECK_EQ(snapshot.frames[1].return_address, 4);
}

TEST(session_shadow_stack_records_nested_callers) {
    Session session;
    std::string error;
    const char* source =
        "start:\n"
        "  CALL middle\n"
        "  HLT\n"
        "middle:\n"
        "  CALL leaf\n"
        "  RET\n"
        "leaf:\n"
        "  RET\n";
    CHECK(session.assemble_source(source, "nested.asm", error));
    CHECK(session.step_instruction());  // CALL middle
    CHECK(session.step_instruction());  // CALL leaf
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.frames.size(), 3u);
    CHECK_EQ(snapshot.frames[0].function, std::string("leaf"));
    CHECK_EQ(snapshot.frames[0].line, 8);
    CHECK_EQ(snapshot.frames[1].function, std::string("middle"));
    CHECK_EQ(snapshot.frames[1].line, 5);
    CHECK_EQ(snapshot.frames[1].pc, 3);
    CHECK_EQ(snapshot.frames[1].return_address, 5);
    CHECK_EQ(snapshot.frames[2].function, std::string("start"));
    CHECK_EQ(snapshot.frames[2].line, 2);
    CHECK_EQ(snapshot.frames[2].pc, 0);
    CHECK_EQ(snapshot.frames[2].return_address, 2);
}

TEST(session_step_instruction_over_a_call) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction());
    CHECK_EQ(session.snapshot().pc, 2);
    CHECK(session.step_instruction_over());
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.pc, 4);
    CHECK_EQ(snapshot.frames.size(), 1u);
    CHECK_EQ(session.console(), std::string("Hi"));
}

TEST(session_hlt_keeps_pc_and_line_on_hlt) {
    Session session;
    std::string error;
    CHECK(session.assemble_source("start:\n  NOP\n  HLT\n", "halt.asm",
                                  error));
    CHECK(session.step_instruction());  // NOP at 0
    CHECK(session.step_instruction());  // HLT at 1
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.pc, 1);
    CHECK_EQ(snapshot.line, 3);
    CHECK(snapshot.message.find("HLT") != std::string::npos);
}

TEST(session_step_instruction_out_of_a_function) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction());
    CHECK(session.step_instruction());  // enter puts
    CHECK_EQ(session.snapshot().frames.size(), 2u);
    CHECK(session.step_instruction_out());
    CHECK_EQ(session.snapshot().pc, 4);
    CHECK_EQ(session.snapshot().frames.size(), 1u);
}

TEST(session_step_line_in) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction());
    CHECK_EQ(session.snapshot().line, 3);
    CHECK(session.step_line(StepMode::LineIn));
    CHECK_EQ(session.snapshot().line, 7);
    CHECK_EQ(session.snapshot().frames.size(), 2u);
}

TEST(session_step_line_over_a_call) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction());
    CHECK_EQ(session.snapshot().pc, 2);
    CHECK(session.step_line(StepMode::LineOver));
    CHECK_EQ(session.snapshot().line, 4);
    CHECK_EQ(session.snapshot().frames.size(), 1u);
    CHECK_EQ(session.console(), std::string("Hi"));
}

TEST(session_step_line_out_of_a_function) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction());
    CHECK(session.step_instruction());  // enter puts
    CHECK_EQ(session.snapshot().line, 7);
    CHECK(session.step_line(StepMode::LineOut));
    CHECK_EQ(session.snapshot().line, 4);
    CHECK_EQ(session.snapshot().frames.size(), 1u);
}

TEST(session_return_without_call_reports_diagnostic) {
    Session session;
    std::string error;
    CHECK(session.assemble_source("start:\n  RET\n", "ret.asm", error));
    CHECK(session.step_instruction());
    const Snapshot snapshot = session.snapshot();
    CHECK(snapshot.message.find("underflow") != std::string::npos);
    CHECK_EQ(snapshot.frames.size(), 1u);
}

TEST(session_step_out_at_top_level_is_a_no_op) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(kCalls, "calls.asm", error));
    CHECK(session.step_instruction_out());
    CHECK_EQ(session.snapshot().pc, 0);
}

TEST(session_recursive_call_frames) {
    Session session;
    std::string error;
    CHECK(session.assemble_source(
        "start:\n"
        "  MOV B, #2\n"
        "  CALL rec\n"
        "  HLT\n"
        "rec:\n"
        "  DEC B\n"
        "  JZ done\n"
        "  CALL rec\n"
        "done:\n"
        "  RET\n",
        "rec.asm", error));
    CHECK(session.step_instruction());  // MOV B
    CHECK(session.step_instruction());  // CALL rec
    CHECK(session.step_instruction());  // DEC B
    CHECK(session.step_instruction());  // JZ not taken
    CHECK(session.step_instruction());  // CALL rec (nested)
    const Snapshot snapshot = session.snapshot();
    CHECK_EQ(snapshot.frames.size(), 3u);
    if (snapshot.frames.size() == 3) {
        CHECK_EQ(snapshot.frames[0].function, std::string("rec"));
        CHECK_EQ(snapshot.frames[1].function, std::string("rec"));
        CHECK_EQ(snapshot.frames[2].function, std::string("start"));
    }
}
