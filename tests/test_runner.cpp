#include <cstdint>
#include <filesystem>
#include <string>

#include "harness.hpp"
#include "kr8/assembler.hpp"
#include "kr8/runner.hpp"

using namespace kr8;

namespace {

AsmResult assemble_example(const std::string& name) {
    const std::filesystem::path path =
        std::filesystem::path(KR8_EXAMPLES_DIR) / name;
    AsmOptions options;
    options.source_name = path.string();
    return assemble_file(path, options);
}

}  // namespace

TEST(runner_hello_output) {
    const AsmResult assembled = assemble_example("hello.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    const RunResult result = run_image(assembled.image);
    CHECK(result.status == RunStatus::Halted);
    CHECK_EQ(result.output, std::string("Hello, KR8!\n"));
    CHECK(result.ok());
    CHECK(result.instructions > 0);
    CHECK(result.cycles > 0);
}

TEST(runner_count_output) {
    const AsmResult assembled = assemble_example("count.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    const RunResult result = run_image(assembled.image);
    CHECK_EQ(result.output, std::string("0123456789\n"));
}

TEST(runner_puts_output) {
    const AsmResult assembled = assemble_example("puts.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    const RunResult result = run_image(assembled.image);
    CHECK_EQ(result.output, std::string("KR8\n"));
}

TEST(runner_updown_input_and_limits) {
    const AsmResult assembled = assemble_example("updown.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    RunOptions options;
    options.input = {'+', '+', '-', '\n', '+'};
    options.max_instructions = 1000;
    const RunResult result = run_image(assembled.image, options);
    CHECK(result.status == RunStatus::Budget);
    CHECK_EQ(result.output,
             std::string("+ up  - down\n5\n6\n7\n6\n7\n"));
}

TEST(runner_stack_output) {
    const AsmResult assembled = assemble_example("stack.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    const RunResult result = run_image(assembled.image);
    CHECK_EQ(result.output, std::string("!A"));
}

TEST(runner_flags_output) {
    const AsmResult assembled = assemble_example("flags.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    const RunResult result = run_image(assembled.image);
    CHECK_EQ(result.output, std::string("G"));
}

TEST(runner_echo_with_input) {
    const AsmResult assembled = assemble_example("echo.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    RunOptions options;
    options.input = {static_cast<std::uint8_t>('a'),
                     static_cast<std::uint8_t>('b'),
                     static_cast<std::uint8_t>('c'),
                     static_cast<std::uint8_t>('\n')};
    const RunResult result = run_image(assembled.image, options);
    CHECK(result.status == RunStatus::Halted);
    CHECK_EQ(result.output, std::string("abc"));
}

TEST(runner_memory_example_writes_buffer) {
    const AsmResult assembled = assemble_example("memory.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    const Symbol* buffer = nullptr;
    for (const auto& symbol : assembled.image.symbols) {
        if (symbol.name == "buf") {
            buffer = &symbol;
            break;
        }
    }
    CHECK(buffer != nullptr);
    if (buffer == nullptr) {
        return;
    }
    const RunResult result = run_image(assembled.image);
    CHECK(result.status == RunStatus::Halted);
    for (int i = 0; i < 8; ++i) {
        CHECK_EQ(result.ram[static_cast<std::size_t>(buffer->value + i)],
                 static_cast<std::uint8_t>(i + 1));
    }
}

TEST(runner_reports_illegal_opcode) {
    Image image;
    image.ram[0] = 0x0C;
    image.mark_code(0);
    image.entry = 0;
    image.image_high = 1;
    const RunResult result = run_image(image);
    CHECK(result.status == RunStatus::Fault);
    CHECK(result.message.find("illegal") != std::string::npos);
    CHECK(!result.ok());
}

TEST(runner_stops_at_brk) {
    Image image;
    image.ram[0] = 0x02;
    image.mark_code(0);
    image.entry = 0;
    image.image_high = 1;
    const RunResult result = run_image(image);
    CHECK(result.status == RunStatus::Break);
    CHECK(result.ok());
}

TEST(runner_reports_stack_overflow) {
    Image image;
    image.ram[0] = 0x04;  // PUSH A
    image.ram[1] = 0x04;  // PUSH A
    image.mark_code(0);
    image.mark_code(1);
    image.entry = 0;
    image.image_high = 0xFF;  // floor at the top: the second push overflows
    const RunResult result = run_image(image);
    CHECK(result.status == RunStatus::Fault);
    CHECK(result.message.find("stack overflow") != std::string::npos);
}

TEST(runner_reports_stack_underflow) {
    Image image;
    image.ram[0] = 0x06;  // POP A with an empty stack
    image.mark_code(0);
    image.entry = 0;
    image.image_high = 1;
    const RunResult result = run_image(image);
    CHECK(result.status == RunStatus::Fault);
    CHECK(result.message.find("stack underflow") != std::string::npos);
}

TEST(runner_reports_pc_wrap) {
    Image image;
    image.ram[0xFF] = 0x0A;  // 2-byte instruction starting at 0xFF
    image.mark_code(0xFF);
    image.entry = 0xFF;
    image.image_high = 256;
    const RunResult result = run_image(image);
    CHECK(result.status == RunStatus::Fault);
    CHECK(result.message.find("end of memory") != std::string::npos);
}

TEST(runner_budget_stops_an_infinite_loop) {
    Image image;
    image.ram[0] = 0x50;  // JMP 0
    image.ram[1] = 0x00;
    image.mark_code(0);
    image.entry = 0;
    image.image_high = 2;
    RunOptions options;
    options.max_instructions = 10;
    const RunResult result = run_image(image, options);
    CHECK(result.status == RunStatus::Budget);
    CHECK_EQ(result.instructions, 10u);
}

TEST(runner_entry_override) {
    Image image;
    image.ram[0x00] = 0x00;  // HLT if entered at 0
    image.ram[0x10] = 0x0A;  // MOV A, #'A'
    image.ram[0x11] = 'A';
    image.ram[0x12] = 0x60;  // OUT A
    image.ram[0x13] = 0x00;  // HLT
    image.mark_code(0x10);
    image.mark_code(0x12);
    image.mark_code(0x13);
    image.entry = 0;
    image.image_high = 0x14;

    RunOptions options;
    options.entry = static_cast<std::uint8_t>(0x10);
    const RunResult result = run_image(image, options);
    CHECK(result.status == RunStatus::Halted);
    CHECK_EQ(result.output, std::string("A"));
}

TEST(runner_trace_contains_instructions) {
    const AsmResult assembled = assemble_example("hello.asm");
    CHECK(assembled.ok);
    if (!assembled.ok) {
        return;
    }
    RunOptions options;
    options.trace = true;
    const RunResult result = run_image(assembled.image, options);
    CHECK(result.trace.find("MOV B, #0x0A") != std::string::npos);
    CHECK(result.trace.find("HLT") != std::string::npos);
}
