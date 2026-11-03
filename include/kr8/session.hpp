#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "kr8/assembler.hpp"
#include "kr8/cpu.hpp"
#include "kr8/disassembler.hpp"
#include "kr8/image.hpp"
#include "kr8/machine.hpp"

namespace kr8 {

enum class StepMode {
    LineIn,
    LineOver,
    LineOut,
};

enum class RunStop {
    Budget,
    Halted,
    Breakpoint,
    Fault,
    Break,
    NotLoaded,
};

struct Frame {
    int index = 0;
    std::uint8_t pc = 0;
    std::uint8_t return_address = 0;
    int line = 0;
    std::string function;
};

struct Snapshot {
    bool loaded = false;
    bool running = false;
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint8_t pc = 0;
    std::uint8_t sp = 0;
    std::uint8_t flags = 0;
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;
    int line = 0;
    std::string function;
    std::string console;
    std::string message;
    std::vector<Frame> frames;
};

/// Portable debug model shared by the windowed UI and the tests. It owns the
/// image, CPU/memory, breakpoints, shadow call stack, console, input, and stats.
class Session {
public:
    Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    bool load_image(const std::filesystem::path& path, std::string& error);
    bool load_image(const Image& image, const std::filesystem::path& path,
                    std::string& error);
    bool assemble(const std::filesystem::path& path, std::string& error);
    bool assemble_source(std::string_view source, const std::string& name,
                         std::string& error);
    bool restart(std::string& error);
    void unload();

    bool loaded() const noexcept { return loaded_; }
    const Image& image() const noexcept { return image_; }
    const std::filesystem::path& image_path() const noexcept {
        return image_path_;
    }

    Snapshot snapshot() const;
    const std::vector<DecodedInstruction>& disassembly() const noexcept {
        return decoded_.instructions;
    }
    const std::vector<std::string>& source_lines() const noexcept {
        return source_lines_;
    }
    const std::set<int>& executable_lines() const noexcept {
        return executable_lines_;
    }
    const std::vector<Symbol>& symbols() const noexcept {
        return image_.symbols;
    }

    int line_at(std::uint8_t pc) const;
    std::string function_at(std::uint8_t pc) const;
    const Symbol* symbol_at(std::uint8_t address) const;
    std::vector<Frame> frames() const;

    bool step_instruction();
    bool step_instruction_over();
    bool step_instruction_out();
    bool step_line(StepMode mode);
    RunStop run_slice(int max_instructions);
    void start_run();
    void stop_run(std::string reason);
    bool running() const noexcept { return running_; }

    const std::set<std::uint8_t>& address_breakpoints() const noexcept {
        return address_breakpoints_;
    }
    const std::set<int>& line_breakpoints() const noexcept {
        return line_breakpoints_;
    }
    void toggle_address_breakpoint(std::uint8_t address);
    void toggle_line_breakpoint(int line);
    void toggle_symbol_breakpoint(const std::string& name);
    bool has_breakpoint(std::uint8_t address) const;
    bool has_line_breakpoint(int line) const;

    void push_input(std::uint8_t value);
    void clear_input();
    std::size_t input_size() const noexcept;
    const std::string& console() const noexcept;
    const std::string& message() const noexcept { return message_; }
    const std::vector<AsmDiagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

    std::uint64_t instructions_executed() const noexcept;
    std::uint64_t cycles() const noexcept;
    std::uint64_t run_time_ms() const;
    void reset_stats();

    void set_register(int index, std::uint8_t value);
    void set_flag(int bit, bool value);
    std::uint8_t read8(std::uint8_t address);
    void write8(std::uint8_t address, std::uint8_t value);

private:
    struct CallRecord {
        std::uint8_t caller_pc = 0;
        std::uint8_t return_address = 0;
        int caller_line = 0;
        std::string caller_function;
    };

    bool load_internal(const Image& image, const std::filesystem::path& path);
    void reset_runtime(bool keep_breakpoints);
    bool step_once();
    bool stopped() const;
    bool at_breakpoint(std::uint8_t pc) const;
    void update_shadow_stack(const StepResult& step, std::uint8_t pc_before);
    void note_stop(std::string message);
    void end_run();
    static std::vector<std::string> split_source(std::string_view text);

    Image image_;
    DecodedImage decoded_;
    std::vector<std::string> source_lines_;
    std::set<int> executable_lines_;
    std::set<std::uint8_t> address_breakpoints_;
    std::set<int> line_breakpoints_;
    Machine machine_;
    std::filesystem::path image_path_;
    std::vector<CallRecord> calls_;
    bool loaded_ = false;
    bool running_ = false;
    bool breakpoint_skip_ = false;
    bool call_stack_valid_ = true;
    std::string message_;
    std::vector<AsmDiagnostic> diagnostics_;
    std::uint64_t accumulated_run_us_ = 0;
    std::chrono::steady_clock::time_point run_started_{};
};

}  // namespace kr8
