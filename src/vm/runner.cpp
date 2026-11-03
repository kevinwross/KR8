#include "kr8/runner.hpp"

#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "kr8/console.hpp"
#include "kr8/cpu.hpp"
#include "kr8/machine.hpp"

namespace kr8 {
namespace {

std::string flags_text(std::uint8_t flags) {
    std::string text;
    text += (flags & FlagZ) != 0 ? 'Z' : '-';
    text += (flags & FlagC) != 0 ? 'C' : '-';
    text += (flags & FlagN) != 0 ? 'N' : '-';
    text += (flags & FlagV) != 0 ? 'V' : '-';
    return text;
}

std::string trace_line(const StepResult& step, const Cpu& cpu) {
    std::ostringstream out;
    out << "pc=" << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << static_cast<int>(step.pc) << " A=" << std::setw(2)
        << static_cast<int>(cpu.a()) << " B=" << std::setw(2)
        << static_cast<int>(cpu.b()) << " SP=" << std::setw(2)
        << static_cast<int>(cpu.sp()) << " F=" << flags_text(cpu.flags())
        << "  " << instruction_text(step.opcode, step.operand);
    return out.str();
}

}  // namespace

RunResult run_image(const Image& image, const RunOptions& options) {
    Machine machine;
    const std::uint8_t entry = options.entry.value_or(image.entry);
    machine.load(image.ram, entry, image.image_high);
    for (const std::uint8_t byte : options.input) {
        machine.push_input(byte);
    }
    if (options.read_stdin) {
        machine.set_fallback_input(read_stdin_byte);
    }

    RunResult result;
    bool stopped = false;
    while (machine.instructions() < options.max_instructions) {
        const StepResult step = machine.step();
        if (options.trace && result.trace.size() < 200000) {
            result.trace += trace_line(step, machine.cpu());
            result.trace += '\n';
        }
        if (step.status == StepStatus::Ok) {
            continue;
        }
        stopped = true;
        if (step.status == StepStatus::Halted) {
            result.status = RunStatus::Halted;
            result.message = "HLT";
        } else if (step.status == StepStatus::Break) {
            result.status = RunStatus::Break;
            result.message = "BRK";
        } else {
            result.status = RunStatus::Fault;
            result.message = machine.fault_message();
        }
        break;
    }
    if (!stopped) {
        result.status = RunStatus::Budget;
        result.message = "instruction budget exceeded";
    }

    result.instructions = machine.instructions();
    result.cycles = machine.cycles();
    result.output = machine.console_output();
    result.ram = machine.memory().bytes();
    return result;
}

}  // namespace kr8
