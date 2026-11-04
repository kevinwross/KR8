#include "kr8/session.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kr8/assembler.hpp"

namespace kr8 {
namespace {

std::string hex_byte(std::uint8_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string text = "0x";
    text += digits[(value >> 4) & 0x0F];
    text += digits[value & 0x0F];
    return text;
}

std::string diagnostic_text(const AsmResult& result) {
    if (result.diagnostics.empty()) {
        return "assembly failed";
    }
    const AsmDiagnostic& diagnostic = result.diagnostics.front();
    std::string text = diagnostic.file;
    if (diagnostic.line > 0) {
        text += ':' + std::to_string(diagnostic.line);
        if (diagnostic.column > 0) {
            text += ':' + std::to_string(diagnostic.column);
        }
    }
    text += ": ";
    text += diagnostic.message;
    return text;
}

}  // namespace

Session::Session() = default;

std::vector<std::string> Session::split_source(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

bool Session::load_image(const std::filesystem::path& path,
                         std::string& error) {
    listing_.clear();
    const ImageLoadResult loaded = load_image_file(path);
    if (!loaded.ok) {
        error = loaded.error;
        return false;
    }
    return load_internal(loaded.image, path);
}

bool Session::load_image(const Image& image, const std::filesystem::path& path,
                         std::string& error) {
    (void)error;
    listing_.clear();
    return load_internal(image, path);
}

bool Session::load_internal(const Image& image,
                            const std::filesystem::path& path) {
    image_ = image;
    image_path_ = path;
    diagnostics_.clear();
    decoded_ = decode_image(image_);
    source_lines_.clear();
    if (!image_.sources.empty()) {
        source_lines_ = split_source(image_.sources.front().text);
    }
    executable_lines_.clear();
    for (const auto& row : image_.lines) {
        if (row.line != 0) {
            executable_lines_.insert(static_cast<int>(row.line));
        }
    }
    address_breakpoints_.clear();
    line_breakpoints_.clear();
    loaded_ = true;
    reset_runtime(false);
    message_ = "loaded";
    return true;
}

bool Session::assemble(const std::filesystem::path& path, std::string& error) {
    AsmOptions options;
    options.source_name = path.string();
    options.include_source = true;
    const AsmResult result = assemble_file(path, options);
    if (!result.ok) {
        listing_.clear();
        diagnostics_ = result.diagnostics;
        error = diagnostic_text(result);
        return false;
    }
    listing_ = result.listing;
    std::filesystem::path output = path;
    output.replace_extension(".k8");
    std::string save_error;
    if (!save_image_file(output, result.image, save_error)) {
        error = save_error;
        return false;
    }
    const bool loaded = load_internal(result.image, path);
    diagnostics_ = result.diagnostics;
    return loaded;
}

bool Session::assemble_source(std::string_view source, const std::string& name,
                              std::string& error) {
    AsmOptions options;
    options.source_name = name;
    options.include_source = true;
    const AsmResult result = kr8::assemble(source, options);
    if (!result.ok) {
        listing_.clear();
        diagnostics_ = result.diagnostics;
        error = diagnostic_text(result);
        return false;
    }
    listing_ = result.listing;
    const bool loaded = load_internal(result.image, name);
    diagnostics_ = result.diagnostics;
    return loaded;
}

bool Session::restart(std::string& error) {
    if (!loaded_) {
        error = "no image is loaded";
        return false;
    }
    reset_runtime(true);
    message_ = "restarted";
    return true;
}

void Session::unload() {
    loaded_ = false;
    image_ = Image{};
    listing_.clear();
    decoded_ = DecodedImage{};
    source_lines_.clear();
    executable_lines_.clear();
    address_breakpoints_.clear();
    line_breakpoints_.clear();
    calls_.clear();
    image_path_.clear();
    running_ = false;
    breakpoint_skip_ = false;
    call_stack_valid_ = true;
    accumulated_run_us_ = 0;
    message_.clear();
}

void Session::reset_runtime(bool keep_breakpoints) {
    machine_.load(image_.ram, image_.entry, image_.image_high);
    calls_.clear();
    call_stack_valid_ = true;
    breakpoint_skip_ = false;
    running_ = false;
    accumulated_run_us_ = 0;
    run_started_ = {};
    if (!keep_breakpoints) {
        address_breakpoints_.clear();
        line_breakpoints_.clear();
    }
    message_ = "ready";
}

int Session::line_at(std::uint8_t pc) const {
    const int exact = image_.line_at(pc);
    if (exact != 0) {
        return exact;
    }
    int best_line = 0;
    std::uint8_t best_address = 0;
    for (const auto& row : image_.lines) {
        if (row.address <= pc && row.line != 0 &&
            (best_line == 0 || row.address >= best_address)) {
            best_line = static_cast<int>(row.line);
            best_address = row.address;
        }
    }
    return best_line;
}

std::string Session::function_at(std::uint8_t pc) const {
    std::string best = "<program>";
    std::int32_t best_value = -1;
    for (const auto& symbol : image_.symbols) {
        if (symbol.kind != SymbolKind::Code) {
            continue;
        }
        if (symbol.value >= 0 && symbol.value <= static_cast<int>(pc) &&
            symbol.value > best_value) {
            best_value = symbol.value;
            best = symbol.name;
        }
    }
    return best;
}

const Symbol* Session::symbol_at(std::uint8_t address) const {
    return image_.find_symbol(address);
}

std::vector<Frame> Session::frames() const {
    std::vector<Frame> result;
    if (!loaded_) {
        return result;
    }
    const Cpu& cpu = machine_.cpu();
    Frame current;
    current.index = 0;
    current.pc = cpu.pc();
    current.line = line_at(cpu.pc());
    current.function = function_at(cpu.pc());
    result.push_back(std::move(current));
    if (!call_stack_valid_) {
        return result;
    }
    int index = 1;
    for (auto it = calls_.rbegin(); it != calls_.rend(); ++it) {
        Frame frame;
        frame.index = index++;
        frame.pc = it->caller_pc;
        frame.return_address = it->return_address;
        frame.line = it->caller_line;
        frame.function = it->caller_function;
        result.push_back(std::move(frame));
    }
    return result;
}

Snapshot Session::snapshot() const {
    Snapshot snapshot;
    snapshot.loaded = loaded_;
    snapshot.running = running_;
    const Cpu& cpu = machine_.cpu();
    snapshot.a = cpu.a();
    snapshot.b = cpu.b();
    snapshot.pc = cpu.pc();
    snapshot.sp = cpu.sp();
    snapshot.flags = cpu.flags();
    snapshot.instructions = cpu.instructions();
    snapshot.cycles = cpu.cycles();
    snapshot.line = line_at(cpu.pc());
    snapshot.function = function_at(cpu.pc());
    snapshot.console = machine_.console_output();
    snapshot.message = message_;
    snapshot.frames = frames();
    return snapshot;
}

bool Session::stopped() const {
    return machine_.cpu().halted() || machine_.cpu().faulted();
}

bool Session::at_breakpoint(std::uint8_t pc) const {
    if (address_breakpoints_.find(pc) != address_breakpoints_.end()) {
        return true;
    }
    const int line = line_at(pc);
    return line != 0 &&
           line_breakpoints_.find(line) != line_breakpoints_.end();
}

void Session::note_stop(std::string message) {
    message_ = std::move(message);
}

void Session::update_shadow_stack(const StepResult& step,
                                  std::uint8_t pc_before) {
    const OpcodeInfo* info = info_for_opcode(step.opcode);
    if (info == nullptr) {
        return;
    }
    if (step.status != StepStatus::Ok) {
        if (info->op == Op::Ret) {
            call_stack_valid_ = false;
        }
        return;
    }
    if (info->op == Op::Call) {
        CallRecord record;
        record.caller_pc = pc_before;
        record.return_address =
            static_cast<std::uint8_t>(pc_before + info->size);
        record.caller_line = line_at(pc_before);
        record.caller_function = function_at(pc_before);
        calls_.push_back(std::move(record));
    } else if (info->op == Op::Ret) {
        if (calls_.empty()) {
            call_stack_valid_ = false;
            note_stop("return without call");
            return;
        }
        const std::uint8_t expected = calls_.back().return_address;
        if (machine_.cpu().pc() != expected) {
            call_stack_valid_ = false;
            note_stop("return address mismatch");
        }
        calls_.pop_back();
    }
}

bool Session::step_once() {
    message_.clear();
    Cpu& cpu = machine_.cpu();
    if (cpu.faulted()) {
        return false;
    }
    if (cpu.halted()) {
        if (cpu.halt_kind() == HaltKind::Break) {
            cpu.resume_from_break();
        } else {
            return false;
        }
    }
    const std::uint8_t pc_before = cpu.pc();
    const StepResult step = machine_.step();
    update_shadow_stack(step, pc_before);
    if (step.status == StepStatus::Halted) {
        note_stop("HLT");
    } else if (step.status == StepStatus::Break) {
        note_stop(step.message);
    } else if (step.status == StepStatus::Fault) {
        note_stop(machine_.fault_message());
    }
    return true;
}

bool Session::step_instruction() {
    if (!loaded_ || running_ || machine_.cpu().faulted()) {
        return false;
    }
    if (machine_.cpu().halted() &&
        machine_.cpu().halt_kind() != HaltKind::Break) {
        return false;
    }
    breakpoint_skip_ = false;
    return step_once();
}

bool Session::step_instruction_over() {
    if (!loaded_ || running_ || machine_.cpu().faulted()) {
        return false;
    }
    if (machine_.cpu().halted() &&
        machine_.cpu().halt_kind() != HaltKind::Break) {
        return false;
    }
    breakpoint_skip_ = false;
    Cpu& cpu = machine_.cpu();
    const OpcodeInfo* info =
        info_for_opcode(machine_.memory().read8(cpu.pc()));
    const bool is_call = info != nullptr && info->op == Op::Call;
    const std::size_t start_depth = calls_.size();
    if (!step_once()) {
        return false;
    }
    if (!is_call) {
        return true;
    }
    for (int i = 0; i < 1'000'000; ++i) {
        if (cpu.faulted() || cpu.halted()) {
            return true;
        }
        if (calls_.size() <= start_depth) {
            return true;
        }
        if (address_breakpoints_.find(cpu.pc()) !=
            address_breakpoints_.end()) {
            note_stop("breakpoint at " + hex_byte(cpu.pc()));
            breakpoint_skip_ = true;
            return true;
        }
        if (!step_once()) {
            return true;
        }
    }
    note_stop("step budget exceeded");
    return false;
}

bool Session::step_instruction_out() {
    if (!loaded_ || running_ || machine_.cpu().faulted()) {
        return false;
    }
    if (machine_.cpu().halted() &&
        machine_.cpu().halt_kind() != HaltKind::Break) {
        return false;
    }
    breakpoint_skip_ = false;
    Cpu& cpu = machine_.cpu();
    const std::size_t start_depth = calls_.size();
    if (start_depth == 0) {
        return true;
    }
    for (int i = 0; i < 1'000'000; ++i) {
        if (cpu.faulted() || cpu.halted()) {
            return true;
        }
        if (calls_.size() < start_depth) {
            return true;
        }
        if (address_breakpoints_.find(cpu.pc()) !=
            address_breakpoints_.end()) {
            note_stop("breakpoint at " + hex_byte(cpu.pc()));
            breakpoint_skip_ = true;
            return true;
        }
        if (!step_once()) {
            return true;
        }
    }
    note_stop("step budget exceeded");
    return false;
}

bool Session::step_line(StepMode mode) {
    if (!loaded_ || running_ || machine_.cpu().faulted()) {
        return false;
    }
    if (machine_.cpu().halted() &&
        machine_.cpu().halt_kind() != HaltKind::Break) {
        return false;
    }
    breakpoint_skip_ = false;
    Cpu& cpu = machine_.cpu();
    const int start_line = line_at(cpu.pc());
    const std::size_t start_depth = calls_.size();
    for (int i = 0; i < 1'000'000; ++i) {
        if (!step_once()) {
            return true;
        }
        if (cpu.faulted() || cpu.halted()) {
            return true;
        }
        const int new_line = line_at(cpu.pc());
        const std::size_t new_depth = calls_.size();
        const bool changed = new_line != 0 && new_line != start_line;
        bool stop = false;
        if (mode == StepMode::LineIn) {
            stop = changed;
        } else if (mode == StepMode::LineOver) {
            stop = changed && new_depth <= start_depth;
        } else {
            stop = changed && new_depth < start_depth;
        }
        if (stop) {
            return true;
        }
        if (changed && line_breakpoints_.find(new_line) !=
                           line_breakpoints_.end()) {
            note_stop("breakpoint at line " + std::to_string(new_line));
            return true;
        }
        if (address_breakpoints_.find(cpu.pc()) !=
            address_breakpoints_.end()) {
            note_stop("breakpoint at " + hex_byte(cpu.pc()));
            return true;
        }
    }
    note_stop("step budget exceeded");
    return false;
}

void Session::start_run() {
    if (!running_) {
        running_ = true;
        run_started_ = std::chrono::steady_clock::now();
    }
}

void Session::end_run() {
    if (running_) {
        const auto now = std::chrono::steady_clock::now();
        accumulated_run_us_ += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - run_started_)
                .count());
        running_ = false;
    }
}

void Session::stop_run(std::string reason) {
    end_run();
    if (!reason.empty()) {
        message_ = std::move(reason);
    }
}

RunStop Session::run_slice(int max_instructions) {
    if (!loaded_) {
        message_ = "no image is loaded";
        return RunStop::NotLoaded;
    }
    Cpu& cpu = machine_.cpu();
    if (cpu.faulted()) {
        end_run();
        message_ = machine_.fault_message();
        return RunStop::Fault;
    }
    if (cpu.halted()) {
        if (cpu.halt_kind() != HaltKind::Break) {
            end_run();
            message_ = "HLT";
            return RunStop::Halted;
        }
        cpu.resume_from_break();
    }
    if (!running_) {
        start_run();
    }
    for (int i = 0; i < max_instructions; ++i) {
        if (!breakpoint_skip_ && at_breakpoint(cpu.pc())) {
            breakpoint_skip_ = true;
            note_stop("breakpoint at " + hex_byte(cpu.pc()));
            end_run();
            return RunStop::Breakpoint;
        }
        breakpoint_skip_ = false;
        if (!step_once()) {
            end_run();
            return cpu.faulted() ? RunStop::Fault : RunStop::Halted;
        }
        if (cpu.faulted()) {
            end_run();
            note_stop(machine_.fault_message());
            return RunStop::Fault;
        }
        if (cpu.halted()) {
            end_run();
            if (cpu.halt_kind() == HaltKind::Break) {
                return RunStop::Break;
            }
            note_stop("HLT");
            return RunStop::Halted;
        }
    }
    return RunStop::Budget;
}

void Session::toggle_address_breakpoint(std::uint8_t address) {
    const auto found = address_breakpoints_.find(address);
    if (found != address_breakpoints_.end()) {
        address_breakpoints_.erase(found);
        note_stop("breakpoint cleared at " + hex_byte(address));
    } else {
        address_breakpoints_.insert(address);
        note_stop("breakpoint set at " + hex_byte(address));
    }
}

void Session::toggle_line_breakpoint(int line) {
    if (line <= 0) {
        return;
    }
    const auto found = line_breakpoints_.find(line);
    if (found != line_breakpoints_.end()) {
        line_breakpoints_.erase(found);
        note_stop("breakpoint cleared at line " + std::to_string(line));
        return;
    }
    int target = line;
    if (executable_lines_.find(target) == executable_lines_.end()) {
        const auto next = executable_lines_.lower_bound(line);
        if (next == executable_lines_.end()) {
            note_stop("no executable line at or after " + std::to_string(line));
            return;
        }
        target = *next;
    }
    line_breakpoints_.insert(target);
    if (target != line) {
        note_stop("line " + std::to_string(line) +
                  " has no code; breakpoint moved to line " +
                  std::to_string(target));
    } else {
        note_stop("breakpoint set at line " + std::to_string(target));
    }
}

void Session::toggle_symbol_breakpoint(const std::string& name) {
    for (const auto& symbol : image_.symbols) {
        if (symbol.name != name) {
            continue;
        }
        if (symbol.kind == SymbolKind::Const || symbol.value < 0 ||
            symbol.value > 255) {
            note_stop("symbol '" + name + "' has no address");
            return;
        }
        toggle_address_breakpoint(static_cast<std::uint8_t>(symbol.value));
        return;
    }
    note_stop("symbol '" + name + "' was not found");
}

bool Session::has_breakpoint(std::uint8_t address) const {
    if (address_breakpoints_.find(address) != address_breakpoints_.end()) {
        return true;
    }
    const int line = line_at(address);
    return line != 0 &&
           line_breakpoints_.find(line) != line_breakpoints_.end();
}

bool Session::has_line_breakpoint(int line) const {
    return line_breakpoints_.find(line) != line_breakpoints_.end();
}

void Session::push_input(std::uint8_t value) {
    machine_.push_input(value);
}

void Session::clear_input() {
    machine_.clear_input();
}

std::size_t Session::input_size() const noexcept {
    return machine_.input_size();
}

const std::string& Session::console() const noexcept {
    return machine_.console_output();
}

std::uint64_t Session::instructions_executed() const noexcept {
    return machine_.cpu().instructions();
}

std::uint64_t Session::cycles() const noexcept {
    return machine_.cpu().cycles();
}

std::uint64_t Session::run_time_ms() const {
    std::uint64_t microseconds = accumulated_run_us_;
    if (running_) {
        const auto now = std::chrono::steady_clock::now();
        microseconds += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - run_started_)
                .count());
    }
    return microseconds / 1000;
}

void Session::reset_stats() {
    end_run();
    machine_.cpu().reset_counters();
    accumulated_run_us_ = 0;
    message_ = "stats reset";
}

void Session::set_register(int index, std::uint8_t value) {
    if (!loaded_) {
        return;
    }
    Cpu& cpu = machine_.cpu();
    switch (index) {
        case 0: cpu.set_a(value); break;
        case 1: cpu.set_b(value); break;
        case 2:
            cpu.set_pc(value);
            note_stop("PC edited");
            break;
        case 3:
            cpu.set_sp(value);
            calls_.clear();
            call_stack_valid_ = false;
            note_stop("SP edited; call stack unavailable");
            break;
        default: break;
    }
}

void Session::set_flag(int bit, bool value) {
    if (bit < 0 || bit > 3) {
        return;
    }
    machine_.cpu().set_flag(static_cast<std::uint8_t>(1u << bit), value);
}

std::uint8_t Session::read8(std::uint8_t address) {
    return machine_.memory().read8(address);
}

void Session::write8(std::uint8_t address, std::uint8_t value) {
    machine_.memory().write8(address, value);
}

}  // namespace kr8
