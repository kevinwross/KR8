#include "app.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "help.hpp"
#include "kr8/disassembler.hpp"
#include "kr8/image.hpp"
#include "kr8/isa.hpp"
#include "kr8/ui/input.hpp"
#include "theme.hpp"

namespace kr8::ui {
namespace {

std::string hex2(std::uint8_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    text.push_back(digits[(value >> 4) & 0x0F]);
    text.push_back(digits[value & 0x0F]);
    return text;
}

void text_right(Canvas& canvas, int right, int y, std::string_view text,
                Color color, FontRole role) {
    const Size size = canvas.measure(text, role);
    canvas.text(right - size.width, y, text, color, role);
}

void text_center(Canvas& canvas, const Rect& rect, std::string_view text,
                 Color color, FontRole role) {
    const Size size = canvas.measure(text, role);
    canvas.text(rect.left + (rect.width() - size.width) / 2,
                rect.top + (rect.height() - canvas.line_height(role)) / 2,
                text, color, role);
}

Color kind_color(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Code: return kAccent;
        case SymbolKind::Data: return kGreen;
        case SymbolKind::Const: return kAmber;
    }
    return kText;
}

bool is_source_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return extension == ".asm" || extension == ".s";
}

std::string extension_of(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return extension;
}

bool read_file_bytes(const std::filesystem::path& path,
                     std::vector<std::uint8_t>& bytes, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "cannot open " + path.string();
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
    return true;
}

std::vector<std::string> console_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::string line;
    for (const char value : text) {
        const auto byte = static_cast<unsigned char>(value);
        if (value == '\n') {
            lines.push_back(line);
            line.clear();
        } else if (value == '\r') {
            continue;
        } else if (byte >= 0x20 && byte <= 0x7E) {
            line.push_back(value);
        } else {
            line += "\\x";
            line += hex2(byte);
        }
    }
    if (!line.empty() || lines.empty()) {
        lines.push_back(line);
    }
    return lines;
}

std::filesystem::path settings_path() {
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr && *appdata != 0) {
        return std::filesystem::path(appdata) / "KR8" / "settings.ini";
    }
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && *xdg != 0) {
        return std::filesystem::path(xdg) / "kr8" / "settings.ini";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != 0) {
        return std::filesystem::path(home) / ".config" / "kr8" /
               "settings.ini";
    }
    return std::filesystem::current_path() / "kr8-settings.ini";
}

void draw_tooltip_box(Canvas& canvas, const Rect& bounds, Point mouse,
                      const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return;
    }
    const int line_height = canvas.line_height(FontRole::MonoSmall) + 2;
    int width = 0;
    for (const auto& line : lines) {
        width = std::max(width,
                         canvas.measure(line, FontRole::MonoSmall).width);
    }
    width += 16;
    const int height = static_cast<int>(lines.size()) * line_height + 10;
    int x = mouse.x + 18;
    int y = mouse.y + 18;
    if (x + width > bounds.right - 4) {
        x = mouse.x - width - 12;
    }
    if (y + height > bounds.bottom - 4) {
        y = mouse.y - height - 12;
    }
    x = std::max(bounds.left + 4, x);
    y = std::max(bounds.top + 4, y);
    const Rect box{x, y, x + width, y + height};
    canvas.fill_rect(box, kPanelAlt);
    canvas.frame_rect(box, kAccent);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        canvas.text(x + 8, y + 6 + static_cast<int>(i) * line_height,
                    lines[i], i == 0 ? kText : kTextDim,
                    FontRole::MonoSmall);
    }
}

}  // namespace

App::App(Window& window, Session& session)
    : window_(window), session_(session) {
    load_settings();
    help_lines_ = kr8::ui::help_lines();
    resize(Size{1280, 820});
    previous_ = session_.snapshot();
    have_previous_ = false;
}

void App::resize(Size size) {
    size_ = size;
    layout();
}

void App::layout() {
    const int width = size_.width;
    const int height = size_.height;
    top_bar_ = Rect{0, 0, width, kTopBar};
    status_ = Rect{0, height - kStatus, width, height};

    const int content_top = kTopBar + kPad;
    const int content_bottom = height - kStatus - kPad;
    int content_height = std::max(240, content_bottom - content_top);
    int main_height = content_height * 55 / 100;
    int bottom_height = content_height - main_height - 6;
    if (bottom_height < 176) {
        bottom_height = 176;
        main_height = content_height - bottom_height - 6;
        if (main_height < 220) {
            main_height = 220;
            bottom_height = std::max(140, content_height - main_height - 6);
        }
    }

    const int available = width - 2 * kPad;
    const int left_width = std::max(320, (available - 6) / 2);
    source_pane_ = Rect{kPad, content_top, kPad + left_width,
                        content_top + main_height};
    disasm_pane_ = Rect{source_pane_.right + 6, content_top, width - kPad,
                        content_top + main_height};

    const int bottom_top = content_top + main_height + 6;
    memory_pane_ = Rect{kPad, bottom_top, kPad + left_width, content_bottom};

    const int right_x = source_pane_.right + 6;
    const int right_width = width - kPad - right_x;
    const int gap = 6;
    const int symbols_width = right_width * 37 / 100;
    const int stack_width = right_width * 22 / 100;
    const int console_width =
        right_width - symbols_width - stack_width - 2 * gap;
    symbols_pane_ = Rect{right_x, bottom_top, right_x + symbols_width,
                         content_bottom};
    stack_pane_ = Rect{symbols_pane_.right + gap, bottom_top,
                       symbols_pane_.right + gap + stack_width, content_bottom};
    console_pane_ = Rect{stack_pane_.right + gap, bottom_top,
                         stack_pane_.right + gap + console_width,
                         content_bottom};

    source_text_ = Rect{source_pane_.left + 1, source_pane_.top + kHeader,
                        source_pane_.right - 1, source_pane_.bottom - 1};
    disasm_text_ = Rect{disasm_pane_.left + 1, disasm_pane_.top + kHeader,
                        disasm_pane_.right - 1,
                        disasm_pane_.bottom - kRegisterHeight};
    register_pane_ = Rect{disasm_pane_.left + 1, disasm_text_.bottom,
                          disasm_pane_.right - 1, disasm_pane_.bottom - 1};
    memory_text_ = Rect{memory_pane_.left + 1, memory_pane_.top + kHeader,
                        memory_pane_.right - 1, memory_pane_.bottom - 1};
    symbols_text_ = Rect{symbols_pane_.left + 1, symbols_pane_.top + kHeader,
                         symbols_pane_.right - 1, symbols_pane_.bottom - 1};
    stack_text_ = Rect{stack_pane_.left + 1, stack_pane_.top + kHeader,
                       stack_pane_.right - 1, stack_pane_.bottom - 1};
    console_text_ = Rect{console_pane_.left + 1, console_pane_.top + kHeader,
                         console_pane_.right - 1, console_pane_.bottom - 28};
    console_input_ = Rect{console_pane_.left + 6, console_pane_.bottom - 26,
                          console_pane_.right - 6, console_pane_.bottom - 4};

    hover_button_ = -1;
    buttons_.clear();
    int x = kPad;
    const int button_y = 6;
    const int button_height = 24;
    auto add_top = [&](const char* label, int width_value, Action action,
                       bool primary) {
        buttons_.push_back(Button{Rect{x, button_y, x + width_value,
                                       button_y + button_height},
                                  label, action, true, primary, 0});
        x += width_value + 6;
    };
    add_top("Load", 62, Action::Load, false);
    add_top("Assemble", 86, Action::Assemble, false);
    add_top("Run", 56, Action::Run, true);
    add_top("Break", 58, Action::Break, false);
    add_top("Restart", 74, Action::Restart, false);
    add_top("Reset stats", 88, Action::ResetStats, false);
    add_top("Settings", 70, Action::Settings, false);

    auto add_group = [&](const Rect& pane, int group, bool source_group) {
        const int button_width = 44;
        const int gap = 4;
        const int start_x = pane.right - 6 - (3 * button_width + 2 * gap);
        const int y = pane.top + 4;
        const Action actions[3] = {
            source_group ? Action::CStepOver : Action::AStepOver,
            source_group ? Action::CStepIn : Action::AStepIn,
            source_group ? Action::CStepOut : Action::AStepOut,
        };
        const char* labels[3] = {"Over", "In", "Out"};
        for (int i = 0; i < 3; ++i) {
            const int left = start_x + i * (button_width + gap);
            buttons_.push_back(Button{
                Rect{left, y, left + button_width, y + 22}, labels[i],
                actions[i], true, false, group});
        }
    };
    add_group(source_pane_, 1, true);
    add_group(disasm_pane_, 2, false);
}

void App::update_buttons() {
    const bool loaded = session_.loaded();
    const bool running = session_.running();
    const bool source = !current_path_.empty() &&
                        is_source_extension(current_path_);
    for (auto& button : buttons_) {
        switch (button.action) {
            case Action::Load:
                button.enabled = true;
                break;
            case Action::Assemble:
                button.enabled = source;
                break;
            case Action::Run:
                button.enabled = loaded && !running;
                break;
            case Action::Break:
                button.enabled = running;
                break;
            case Action::Restart:
            case Action::ResetStats:
                button.enabled = loaded;
                break;
            case Action::Settings:
                button.enabled = true;
                break;
            case Action::CStepOver:
            case Action::CStepIn:
            case Action::CStepOut:
            case Action::AStepOver:
            case Action::AStepIn:
            case Action::AStepOut:
                button.enabled = loaded && !running;
                break;
            default:
                break;
        }
    }
}

void App::paint(Canvas& canvas) {
    canvas.fill_rect(Rect{0, 0, size_.width, size_.height}, kBg);
    update_buttons();
    const Snapshot snapshot = session_.snapshot();
    draw_top(canvas, snapshot);
    draw_source(canvas, snapshot);
    draw_disassembly(canvas, snapshot);
    draw_memory(canvas, snapshot);
    draw_symbols(canvas, snapshot);
    draw_stack(canvas, snapshot);
    draw_console(canvas, snapshot);
    draw_status(canvas, snapshot);
    draw_tooltip(canvas, snapshot);
    if (help_open_) {
        draw_help(canvas);
    } else if (settings_open_) {
        draw_settings(canvas);
    } else if (errors_open_) {
        draw_errors(canvas);
    }
}

void App::draw_button(Canvas& canvas, const Button& button) {
    const bool hovered = hover_button_ >= 0 &&
                         &buttons_[static_cast<std::size_t>(hover_button_)] ==
                             &button;
    const Color fill =
        !button.enabled ? kButtonOff
                        : (hovered ? kButtonHover
                                   : (button.primary ? kAccentDim : kButton));
    canvas.fill_rect(button.rect, fill);
    canvas.frame_rect(button.rect, button.enabled ? kBorder : kPanelAlt);
    text_center(canvas, button.rect, button.label,
                button.enabled ? kText : kTextDim, FontRole::Ui);
}

void App::draw_pane_header(Canvas& canvas, const Rect& pane,
                           std::string_view title, int group) {
    const Rect header{pane.left, pane.top, pane.right,
                      pane.top + kHeader};
    canvas.fill_rect(header, kPanelAlt);
    canvas.fill_rect(Rect{pane.left, header.bottom - 1, pane.right,
                          header.bottom},
                     kBorder);
    canvas.text(pane.left + 10, pane.top + 5, title, kAccent,
                FontRole::UiBold);
    for (const auto& button : buttons_) {
        if (button.group == group) {
            draw_button(canvas, button);
        }
    }
}

void App::draw_top(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(top_bar_, kPanel);
    canvas.fill_rect(Rect{0, top_bar_.bottom - 1, top_bar_.right,
                          top_bar_.bottom},
                     kBorder);
    for (const auto& button : buttons_) {
        if (button.group == 0) {
            draw_button(canvas, button);
        }
    }

    const std::uint64_t elapsed = session_.run_time_ms();
    const std::uint64_t rate =
        elapsed > 0 ? (snapshot.instructions * 1000) / elapsed : 0;
    const int y = 10;
    text_right(canvas, size_.width - 10, y,
               "INS " + std::to_string(snapshot.instructions), kTextDim,
               FontRole::MonoSmall);
    text_right(canvas, size_.width - 130, y,
               "CYC " + std::to_string(snapshot.cycles), kTextDim,
               FontRole::MonoSmall);
    text_right(canvas, size_.width - 255, y,
               "TIME " + std::to_string(elapsed) + "ms", kTextDim,
               FontRole::MonoSmall);
    text_right(canvas, size_.width - 390, y,
               "IPS " + std::to_string(rate), kTextDim,
               FontRole::MonoSmall);
}

int App::source_line_at(int y) const {
    if (y < source_text_.top || y >= source_text_.bottom) {
        return 0;
    }
    const int row = (y - source_text_.top) / kRowHeight;
    const int index = source_scroll_ + row;
    if (index < 0 ||
        index >= static_cast<int>(session_.source_lines().size())) {
        return 0;
    }
    return index + 1;
}

void App::draw_source(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(source_pane_, kPanel);
    canvas.frame_rect(source_pane_, kBorder);
    draw_pane_header(canvas, source_pane_, "Source  (line steps)", 1);

    if (!session_.loaded()) {
        canvas.text(source_text_.left + 12, source_text_.top + 12,
                    "Load a .asm or .k8 file (Ctrl+O).", kTextDim,
                    FontRole::MonoSmall);
        return;
    }
    const auto& lines = session_.source_lines();
    if (lines.empty()) {
        canvas.text(source_text_.left + 12, source_text_.top + 12,
                    "No source attached; use the disassembly.", kTextDim,
                    FontRole::MonoSmall);
        return;
    }

    const int visible = std::max(1, source_text_.height() / kRowHeight);
    if (follow_ && snapshot.line > 0) {
        const int row = snapshot.line - 1;
        if (row < source_scroll_) {
            source_scroll_ = std::max(0, row - 2);
        } else if (row >= source_scroll_ + visible) {
            source_scroll_ = row - visible + 3;
        }
    }
    source_scroll_ =
        std::clamp(source_scroll_, 0,
                   std::max(0, static_cast<int>(lines.size()) - visible));

    const Rect gutter{source_text_.left, source_text_.top,
                      source_text_.left + kSourceGutter, source_text_.bottom};
    canvas.fill_rect(gutter, kPanelAlt);
    canvas.fill_rect(Rect{gutter.right - 1, gutter.top, gutter.right,
                          gutter.bottom},
                     kBorder);
    canvas.push_clip(source_text_);
    for (int row = 0; row < visible; ++row) {
        const int index = source_scroll_ + row;
        if (index >= static_cast<int>(lines.size())) {
            break;
        }
        const int line_number = index + 1;
        const int y = source_text_.top + row * kRowHeight;
        const bool current = line_number == snapshot.line;
        if (current) {
            canvas.fill_rect(
                Rect{source_text_.left, y, source_text_.right,
                     y + kRowHeight},
                kHighlight);
            canvas.fill_rect(
                Rect{source_text_.left, y, source_text_.left + 3,
                     y + kRowHeight},
                kAccent);
        }
        text_right(canvas, gutter.right - 6, y + 2,
                   std::to_string(line_number),
                   current ? kAccent : kTextDim, FontRole::MonoSmall);
        if (session_.has_line_breakpoint(line_number)) {
            canvas.fill_rect(
                Rect{gutter.left + 5, y + 7, gutter.left + 11, y + 13},
                kRed);
        }
        const bool executable =
            session_.executable_lines().count(line_number) != 0;
        canvas.text(source_text_.left + kSourceGutter + 6, y + 1,
                    lines[static_cast<std::size_t>(index)],
                    executable ? kText : kTextDim, FontRole::MonoSmall);
    }
    canvas.pop_clip();
}

int App::asm_row_at(int y) const {
    if (y < disasm_text_.top || y >= disasm_text_.bottom) {
        return -1;
    }
    const int row = (y - disasm_text_.top) / kRowHeight;
    const int index = asm_scroll_ + row;
    if (index < 0 ||
        index >= static_cast<int>(session_.disassembly().size())) {
        return -1;
    }
    return index;
}

void App::draw_disassembly(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(disasm_pane_, kPanel);
    canvas.frame_rect(disasm_pane_, kBorder);
    draw_pane_header(canvas, disasm_pane_, "Disassembly  (instruction steps)",
                     2);

    const auto& instructions = session_.disassembly();
    if (!session_.loaded() || instructions.empty()) {
        canvas.text(disasm_text_.left + 12, disasm_text_.top + 12,
                    "No image loaded.", kTextDim, FontRole::MonoSmall);
    } else {
        int current_index = -1;
        for (std::size_t i = 0; i < instructions.size(); ++i) {
            if (instructions[i].address == snapshot.pc) {
                current_index = static_cast<int>(i);
                break;
            }
        }
        const int visible =
            std::max(1, disasm_text_.height() / kRowHeight);
        if (current_index < 0) {
            for (std::size_t i = 0; i < instructions.size(); ++i) {
                if (instructions[i].address <= snapshot.pc) {
                    current_index = static_cast<int>(i);
                }
            }
        }
        if (follow_ && current_index >= 0) {
            if (current_index < asm_scroll_) {
                asm_scroll_ = std::max(0, current_index - 2);
            } else if (current_index >= asm_scroll_ + visible) {
                asm_scroll_ = current_index - visible + 3;
            }
        }
        asm_scroll_ = std::clamp(
            asm_scroll_, 0,
            std::max(0, static_cast<int>(instructions.size()) - visible));

        asm_rows_.clear();
        canvas.push_clip(disasm_text_);
        for (int row = 0; row < visible; ++row) {
            const int index = asm_scroll_ + row;
            if (index >= static_cast<int>(instructions.size())) {
                break;
            }
            const auto& instruction =
                instructions[static_cast<std::size_t>(index)];
            const int y = disasm_text_.top + row * kRowHeight;
            const Rect row_rect{disasm_text_.left, y, disasm_text_.right,
                                y + kRowHeight};
            const Rect gutter{disasm_text_.left, y,
                              disasm_text_.left + kAsmGutter, y + kRowHeight};
            AsmRow hit;
            hit.index = index;
            hit.address = instruction.address;
            hit.rect = row_rect;
            hit.gutter = gutter;
            asm_rows_.push_back(hit);

            const bool current = index == current_index;
            if (current) {
                canvas.fill_rect(row_rect, kHighlight);
                canvas.fill_rect(
                    Rect{disasm_text_.left, y, disasm_text_.left + 3,
                         y + kRowHeight},
                    kAccent);
            }
            if (session_.address_breakpoints().count(
                    instruction.address) != 0) {
                canvas.fill_rect(
                    Rect{gutter.left + 3, y + 7, gutter.left + 9, y + 13},
                    kRed);
            }
            canvas.text(disasm_text_.left + 22, y + 1,
                        hex2(instruction.address),
                        current ? kText : kTextDim, FontRole::MonoSmall);
            std::string bytes;
            for (int byte = 0; byte < instruction.size; ++byte) {
                if (!bytes.empty()) {
                    bytes.push_back(' ');
                }
                bytes += hex2(instruction.bytes[static_cast<std::size_t>(
                    byte)]);
            }
            canvas.text(disasm_text_.left + 54, y + 1, bytes,
                        current ? kText : kTextDim, FontRole::MonoSmall);
            canvas.text(disasm_text_.left + 108, y + 1, instruction.text,
                        current ? kText : kText, FontRole::MonoSmall);
        }
        canvas.pop_clip();
    }

    draw_registers(canvas, snapshot);
}

void App::draw_registers(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(register_pane_, kPanelAlt);
    canvas.fill_rect(Rect{register_pane_.left, register_pane_.top,
                          register_pane_.right, register_pane_.top + 1},
                     kBorder);
    canvas.text(register_pane_.left + 10, register_pane_.top + 4,
                "Registers", kAccent, FontRole::UiBold);

    const char* names[4] = {"A", "B", "PC", "SP"};
    const std::uint8_t values[4] = {snapshot.a, snapshot.b, snapshot.pc,
                                    snapshot.sp};
    const std::uint8_t old_values[4] = {
        previous_.a, previous_.b, previous_.pc, previous_.sp};
    const int column_width = (register_pane_.width() - 20) / 4;
    for (int i = 0; i < 4; ++i) {
        const int x = register_pane_.left + 10 + i * column_width;
        canvas.text(x, register_pane_.top + 24, names[i], kTextDim,
                    FontRole::MonoSmall);
        register_rects_[i] = Rect{x + 20, register_pane_.top + 22,
                                  x + column_width - 8,
                                  register_pane_.top + 42};
        if (editing_register_ == i) {
            canvas.fill_rect(register_rects_[i], kHighlight);
            canvas.frame_rect(register_rects_[i], kAccent);
            canvas.text(x + 24, register_pane_.top + 25,
                        edit_text_.empty() ? "_" : edit_text_, kText,
                        FontRole::MonoSmall);
        } else {
            const bool changed =
                show_changes_ && have_previous_ && values[i] != old_values[i];
            canvas.text(x + 24, register_pane_.top + 25,
                        hex2(values[i]), changed ? kGreen : kText,
                        FontRole::MonoSmall);
        }
    }

    const int flags_y = register_pane_.top + 54;
    canvas.text(register_pane_.left + 10, flags_y, "FLAGS", kTextDim,
                FontRole::MonoSmall);
    const char* flag_names[4] = {"Z", "C", "N", "V"};
    for (int i = 0; i < 4; ++i) {
        const int left = register_pane_.left + 62 + i * 30;
        flag_rects_[i] = Rect{left, flags_y - 2, left + 18, flags_y + 16};
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << i);
        const bool set = (snapshot.flags & mask) != 0;
        const bool changed = show_changes_ && have_previous_ &&
                             (snapshot.flags & mask) !=
                                 (previous_.flags & mask);
        canvas.fill_rect(flag_rects_[i], set ? kGreen : kPanel);
        canvas.frame_rect(flag_rects_[i], changed ? kAmber : kBorder);
        text_center(canvas, flag_rects_[i], flag_names[i],
                    set ? kBg : kTextDim, FontRole::MonoSmall);
    }
}

int App::memory_cell_at(Point point) const {
    for (std::size_t i = 0; i < memory_cells_.size(); ++i) {
        if (memory_cells_[i].rect.contains(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void App::draw_memory(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(memory_pane_, kPanel);
    canvas.frame_rect(memory_pane_, kBorder);
    draw_pane_header(canvas, memory_pane_, "Memory", 0);

    if (!session_.loaded()) {
        canvas.text(memory_text_.left + 12, memory_text_.top + 12,
                    "No image loaded.", kTextDim, FontRole::MonoSmall);
        return;
    }

    const int char_width = std::max(
        1, canvas.measure("0", FontRole::MonoSmall).width);
    const int address_width = 26;
    const int ascii_width = 16 * char_width + 8;
    int cell_width =
        (memory_text_.width() - address_width - ascii_width - 8) / 16;
    cell_width = std::clamp(cell_width, 14, 26);
    const int row_height =
        std::max(11, (memory_text_.height() - 18) / 16);
    const int grid_left = memory_text_.left + 4;
    const int grid_top = memory_text_.top + 18;
    const int ascii_left =
        grid_left + address_width + 16 * cell_width + 8;

    for (int column = 0; column < 16; ++column) {
        canvas.text(grid_left + address_width + column * cell_width,
                    memory_text_.top + 2, hex2(static_cast<std::uint8_t>(
                                                         column)),
                    kTextDim, FontRole::MonoSmall);
    }

    memory_cells_.clear();
    for (int row = 0; row < 16; ++row) {
        const int y = grid_top + row * row_height;
        canvas.text(grid_left, y + 1,
                    hex2(static_cast<std::uint8_t>(row * 16)), kTextDim,
                    FontRole::MonoSmall);
        std::string ascii;
        for (int column = 0; column < 16; ++column) {
            const auto address =
                static_cast<std::uint8_t>(row * 16 + column);
            const int x =
                grid_left + address_width + column * cell_width;
            const Rect cell{x, y, x + cell_width, y + row_height};
            memory_cells_.push_back(MemoryCell{cell, address});
            if (address == snapshot.pc) {
                canvas.fill_rect(cell, kAccentDim);
            } else if (address == snapshot.sp) {
                canvas.fill_rect(cell, kStmt);
            } else if (selected_memory_address_ == static_cast<int>(address)) {
                canvas.fill_rect(cell, kStmt);
            }
            const std::uint8_t value = session_.read8(address);
            if (editing_memory_ == static_cast<int>(address)) {
                canvas.fill_rect(cell, kHighlight);
                canvas.frame_rect(cell, kAccent);
                canvas.text(x + 1, y + 1,
                            edit_text_.empty() ? "_" : edit_text_, kText,
                            FontRole::MonoSmall);
            } else {
                canvas.text(x + 1, y + 1, hex2(value),
                            address == snapshot.pc ? kText : kText,
                            FontRole::MonoSmall);
            }
            ascii.push_back(value >= 0x20 && value <= 0x7E
                                ? static_cast<char>(value)
                                : '.');
        }
        canvas.text(ascii_left, y + 1, ascii, kTextDim,
                    FontRole::MonoSmall);
    }
}

int App::symbol_row_at(Point point) const {
    for (const auto& row : symbol_rows_) {
        if (row.rect.contains(point)) {
            return row.index;
        }
    }
    return -1;
}

void App::draw_symbols(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(symbols_pane_, kPanel);
    canvas.frame_rect(symbols_pane_, kBorder);
    draw_pane_header(canvas, symbols_pane_, "Symbols", 0);

    const auto& symbols = session_.symbols();
    if (symbols.empty()) {
        canvas.text(symbols_text_.left + 8, symbols_text_.top + 8,
                    "No symbols.", kTextDim, FontRole::MonoSmall);
        return;
    }
    const int row_height = 18;
    const int visible = std::max(1, symbols_text_.height() / row_height);
    symbols_scroll_ = std::clamp(
        symbols_scroll_, 0,
        std::max(0, static_cast<int>(symbols.size()) - visible));
    symbol_rows_.clear();
    canvas.push_clip(symbols_text_);
    for (int row = 0; row < visible; ++row) {
        const int index = symbols_scroll_ + row;
        if (index >= static_cast<int>(symbols.size())) {
            break;
        }
        const auto& symbol = symbols[static_cast<std::size_t>(index)];
        const int y = symbols_text_.top + row * row_height;
        const Rect rect{symbols_text_.left, y, symbols_text_.right,
                        y + row_height};
        symbol_rows_.push_back(HitRow{rect, index});
        if (symbol.kind == SymbolKind::Code &&
            symbol.value == static_cast<std::int32_t>(snapshot.pc)) {
            canvas.fill_rect(rect, kHighlight);
        }
        if (symbol.kind != SymbolKind::Const && symbol.value >= 0 &&
            symbol.value <= 255 &&
            session_.address_breakpoints().count(
                static_cast<std::uint8_t>(symbol.value)) != 0) {
            canvas.fill_rect(Rect{rect.left + 3, y + 7, rect.left + 8,
                                  y + 12},
                             kRed);
        }
        canvas.text(symbols_text_.left + 12, y + 1, symbol.name,
                    kind_color(symbol.kind), FontRole::MonoSmall);
        text_right(canvas, symbols_text_.right - 6, y + 1,
                   hex2(static_cast<std::uint8_t>(symbol.value & 0xFF)),
                   kTextDim, FontRole::MonoSmall);
    }
    canvas.pop_clip();
}

int App::stack_row_at(Point point) const {
    for (const auto& row : stack_rows_) {
        if (row.rect.contains(point)) {
            return row.index;
        }
    }
    return -1;
}

void App::draw_stack(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(stack_pane_, kPanel);
    canvas.frame_rect(stack_pane_, kBorder);
    draw_pane_header(canvas, stack_pane_, "Call stack", 0);

    if (snapshot.frames.empty()) {
        canvas.text(stack_text_.left + 8, stack_text_.top + 8,
                    "No frames.", kTextDim, FontRole::MonoSmall);
        return;
    }
    const int row_height = 20;
    const int visible = std::max(1, stack_text_.height() / row_height);
    stack_scroll_ = std::clamp(
        stack_scroll_, 0,
        std::max(0, static_cast<int>(snapshot.frames.size()) - visible));
    stack_rows_.clear();
    canvas.push_clip(stack_text_);
    for (int row = 0; row < visible; ++row) {
        const int index = stack_scroll_ + row;
        if (index >= static_cast<int>(snapshot.frames.size())) {
            break;
        }
        const Frame& frame = snapshot.frames[static_cast<std::size_t>(index)];
        const int y = stack_text_.top + row * row_height;
        const Rect rect{stack_text_.left, y, stack_text_.right,
                        y + row_height};
        stack_rows_.push_back(HitRow{rect, index});
        if (frame.index == 0) {
            canvas.fill_rect(rect, kStmt);
        }
        const std::string line =
            "#" + std::to_string(frame.index) + " " + frame.function +
            " line " + std::to_string(frame.line);
        canvas.text(stack_text_.left + 6, y + 2, line,
                    frame.index == 0 ? kText : kTextDim,
                    FontRole::MonoSmall);
        if (frame.return_address != 0) {
            text_right(canvas, stack_text_.right - 6, y + 2,
                       "ret " + hex2(frame.return_address), kTextDim,
                       FontRole::MonoSmall);
        }
    }
    canvas.pop_clip();
}

void App::draw_console(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(console_pane_, kPanel);
    canvas.frame_rect(console_pane_, kBorder);
    draw_pane_header(canvas, console_pane_, "Console", 0);

    const std::vector<std::string> lines = console_lines(snapshot.console);
    const int row_height = 16;
    const int visible = std::max(1, console_text_.height() / row_height);
    console_scroll_ = std::clamp(
        console_scroll_, 0,
        std::max(0, static_cast<int>(lines.size()) - visible));
    const int start =
        std::max(0, static_cast<int>(lines.size()) - visible - console_scroll_);
    canvas.push_clip(console_text_);
    for (int row = 0; row < visible; ++row) {
        const int index = start + row;
        if (index >= static_cast<int>(lines.size())) {
            break;
        }
        canvas.text(console_text_.left + 6,
                    console_text_.top + row * row_height,
                    lines[static_cast<std::size_t>(index)], kText,
                    FontRole::MonoSmall);
    }
    canvas.pop_clip();

    console_field_.focused = console_focus_;
    text_field_paint(canvas, console_input_, console_field_,
                     "input (Enter sends)");
}

std::string App::status_text(const Snapshot& snapshot) const {
    std::ostringstream out;
    out << "pc=0x" << hex2(snapshot.pc) << "  line " << snapshot.line
        << "  instr " << snapshot.instructions << "  cycles "
        << snapshot.cycles;
    out << (snapshot.running ? "  [running]" : "  [paused]");
    return out.str();
}

void App::draw_status(Canvas& canvas, const Snapshot& snapshot) {
    canvas.fill_rect(status_, kPanel);
    canvas.fill_rect(Rect{0, status_.top, status_.right, status_.top + 1},
                     kBorder);
    canvas.text(status_.left + 8, status_.top + 4, status_text(snapshot),
                kTextDim, FontRole::MonoSmall);
    const std::string message =
        !message_.empty() ? message_ : snapshot.message;
    if (!message.empty()) {
        canvas.push_clip(Rect{status_.left + 370, status_.top,
                              status_.right - 330, status_.bottom});
        canvas.text(status_.left + 380, status_.top + 4, "   " + message,
                    message.find("error") != std::string::npos ||
                            message.find("fault") != std::string::npos
                        ? kRed
                        : kTextDim,
                    FontRole::MonoSmall);
        canvas.pop_clip();
    }
    text_right(canvas, status_.right - 8, status_.top + 4,
               "F5 run  F6 break  F10 line  F11 step  F1 help", kTextDim,
               FontRole::MonoSmall);
}

void App::draw_help(Canvas& canvas) {
    const Rect overlay{kPad, top_bar_.bottom + kPad, size_.width - kPad,
                       status_.top - kPad};
    canvas.fill_rect(overlay, kPanel);
    canvas.frame_rect(overlay, kAccent, 2);
    canvas.text(overlay.left + 14, overlay.top + 10, "KR8 Help", kAccent,
                FontRole::UiBold);
    canvas.text(overlay.right - 160, overlay.top + 10,
                "Esc or F1 to close", kTextDim, FontRole::MonoSmall);

    const int line_height = 15;
    const int column_width = (overlay.width() - 40) / 2;
    const int split = static_cast<int>(help_lines_.size()) / 2;
    canvas.push_clip(Rect{overlay.left + 10, overlay.top + 34,
                          overlay.right - 10, overlay.bottom - 10});
    for (int i = 0; i < static_cast<int>(help_lines_.size()); ++i) {
        const int column = i < split ? 0 : 1;
        const int row = column == 0 ? i : i - split;
        const int x = overlay.left + 14 + column * column_width;
        const int y = overlay.top + 38 + row * line_height;
        canvas.text(x, y, help_lines_[static_cast<std::size_t>(i)],
                    column == 0 ? kText : kTextDim,
                    FontRole::MonoSmall);
    }
    canvas.pop_clip();
}

int App::hit_button(Point point) const {
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        if (buttons_[i].enabled && buttons_[i].rect.contains(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void App::draw_tooltip(Canvas& canvas, const Snapshot& snapshot) {
    if (help_open_ || settings_open_ || errors_open_) {
        return;
    }
    std::vector<std::string> lines;
    if (hover_asm_ >= 0 &&
        hover_asm_ < static_cast<int>(asm_rows_.size())) {
        const int index = asm_rows_[static_cast<std::size_t>(hover_asm_)].index;
        const auto& instructions = session_.disassembly();
        if (index >= 0 && index < static_cast<int>(instructions.size())) {
            const auto& instruction =
                instructions[static_cast<std::size_t>(index)];
            lines.push_back(instruction.text);
            const OpcodeInfo* info =
                info_for_opcode(instruction.bytes[0]);
            if (info != nullptr) {
                lines.emplace_back(info->help);
            }
            lines.emplace_back("Operands: #imm, [addr], [B]");
        }
    } else if (hover_memory_ >= 0 &&
               hover_memory_ < static_cast<int>(memory_cells_.size())) {
        const auto address =
            memory_cells_[static_cast<std::size_t>(hover_memory_)].address;
        const std::uint8_t value = session_.read8(address);
        lines.push_back("Byte 0x" + hex2(address) + " = 0x" + hex2(value) +
                        "  (" + std::to_string(value) + ")");
        const Symbol* symbol = session_.symbol_at(address);
        if (symbol != nullptr) {
            lines.push_back(symbol->name + "  (" +
                            (symbol->kind == SymbolKind::Code
                                 ? "code"
                                 : symbol->kind == SymbolKind::Data ? "data"
                                                                    : "const") +
                            ")");
        } else {
            lines.push_back("PC " + hex2(snapshot.pc) + "   SP " +
                            hex2(snapshot.sp));
        }
    }
    if (!lines.empty()) {
        draw_tooltip_box(canvas,
                         Rect{0, top_bar_.bottom, size_.width, status_.top},
                         mouse_, lines);
    }
}

int App::error_row_at(Point point) const {
    for (const auto& row : error_rows_) {
        if (row.rect.contains(point)) {
            return row.index;
        }
    }
    return -1;
}

void App::draw_errors(Canvas& canvas) {
    const Rect overlay{kPad, top_bar_.bottom + kPad, size_.width - kPad,
                       status_.top - kPad};
    canvas.fill_rect(overlay, kPanel);
    canvas.frame_rect(overlay, kRed, 2);
    canvas.text(overlay.left + 14, overlay.top + 10, "Assembly errors",
                kRed, FontRole::UiBold);
    canvas.text(overlay.right - 210, overlay.top + 10,
                "click a line to jump; Esc closes", kTextDim,
                FontRole::MonoSmall);

    error_rows_.clear();
    const int line_height = 18;
    int y = overlay.top + 40;
    const int max_rows = std::max(1, (overlay.height() - 52) / line_height);
    int shown = 0;
    for (std::size_t i = 0; i < diagnostics_.size() && shown < max_rows;
         ++i) {
        const auto& diagnostic = diagnostics_[i];
        std::string text = diagnostic.file;
        if (diagnostic.line > 0) {
            text += ':' + std::to_string(diagnostic.line);
            if (diagnostic.column > 0) {
                text += ':' + std::to_string(diagnostic.column);
            }
        }
        text += ": ";
        text += diagnostic.message;
        const Rect row{overlay.left + 10, y, overlay.right - 10,
                       y + line_height};
        error_rows_.push_back(HitRow{row, static_cast<int>(i)});
        canvas.text(row.left + 4, y + 2, text,
                    diagnostic.warning ? kAmber : kRed,
                    FontRole::MonoSmall);
        y += line_height;
        ++shown;
    }
    if (diagnostics_.size() > static_cast<std::size_t>(shown)) {
        canvas.text(overlay.left + 14, y + 2,
                    "... and " +
                        std::to_string(diagnostics_.size() - shown) +
                        " more",
                    kTextDim, FontRole::MonoSmall);
    }
}

int App::settings_row_at(Point point) const {
    for (const auto& row : settings_rows_) {
        if (row.rect.contains(point)) {
            return row.index;
        }
    }
    return -1;
}

void App::draw_settings(Canvas& canvas) {
    const int width = 440;
    const int height = 230;
    const int left = (size_.width - width) / 2;
    const int top = (size_.height - height) / 2;
    const Rect overlay{left, top, left + width, top + height};
    canvas.fill_rect(overlay, kPanel);
    canvas.frame_rect(overlay, kAccent, 2);
    canvas.text(overlay.left + 14, overlay.top + 10, "Settings", kAccent,
                FontRole::UiBold);
    canvas.text(overlay.right - 120, overlay.top + 10,
                "Esc to close", kTextDim, FontRole::MonoSmall);

    settings_rows_.clear();
    auto row_rect = [&](int index) {
        return Rect{overlay.left + 14, overlay.top + 48 + index * 34,
                    overlay.right - 14, overlay.top + 76 + index * 34};
    };
    const std::string follow_mark = follow_ ? "[x]" : "[ ]";
    const std::string changes_mark = show_changes_ ? "[x]" : "[ ]";
    std::vector<std::string> labels = {
        follow_mark + "  Follow PC while stepping and running",
        changes_mark + "  Highlight changed registers and flags",
        "Run slice: 50,000 instructions",
        "Close",
    };
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        const Rect row = row_rect(i);
        if (i == 3) {
            const Rect button{row.left, row.top, row.left + 90, row.bottom};
            canvas.fill_rect(button, kButton);
            canvas.frame_rect(button, kBorder);
            text_center(canvas, button, labels[i], kText, FontRole::Ui);
            settings_rows_.push_back(HitRow{button, i});
            continue;
        }
        if (i < 2) {
            canvas.fill_rect(row, kPanelAlt);
        }
        canvas.text(row.left + 8, row.top + 5, labels[i], kText,
                    FontRole::MonoSmall);
        if (i < 2) {
            settings_rows_.push_back(HitRow{row, i});
        }
    }
}

bool App::in_source_gutter(Point point) const {
    return source_text_.contains(point) &&
           point.x < source_text_.left + kSourceGutter;
}

bool App::in_asm_gutter(Point point) const {
    return disasm_text_.contains(point) &&
           point.x < disasm_text_.left + kAsmGutter;
}

void App::activate(Action action) {
    switch (action) {
        case Action::Load:
            open_file_dialog();
            break;
        case Action::Assemble:
            assemble_current();
            break;
        case Action::Run:
            start_run();
            break;
        case Action::Break:
            stop_run("paused");
            break;
        case Action::Restart: {
            std::string error;
            if (session_.loaded() && session_.restart(error)) {
                message_ = "restarted";
                follow_ = true;
                diagnostics_.clear();
                errors_open_ = false;
                update_snapshot_history();
            } else {
                message_ = error;
            }
            window_.set_timer(8, false);
            break;
        }
        case Action::ResetStats:
            session_.reset_stats();
            message_ = "stats reset";
            break;
        case Action::Settings:
            open_settings();
            break;
        case Action::CStepOver:
            step_line(StepMode::LineOver);
            break;
        case Action::CStepIn:
            step_line(StepMode::LineIn);
            break;
        case Action::CStepOut:
            step_line(StepMode::LineOut);
            break;
        case Action::AStepOver:
            step_instruction(0);
            break;
        case Action::AStepIn:
            step_instruction(1);
            break;
        case Action::AStepOut:
            step_instruction(2);
            break;
        default:
            break;
    }
    window_.invalidate();
}

void App::open_file_dialog() {
    window_.open_file(
        [this](std::optional<std::filesystem::path> path) {
            if (path.has_value()) {
                open(*path);
            }
        },
        "Open KR8 program", "*.asm;*.s;*.k8;*.bin");
}

void App::open_settings() {
    settings_open_ = !settings_open_;
    help_open_ = false;
    errors_open_ = false;
    window_.invalidate();
}

void App::load_settings() {
    std::ifstream input(settings_path());
    if (!input) {
        return;
    }
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, equals);
        const bool enabled = line.substr(equals + 1) != "0";
        if (key == "follow") {
            follow_ = enabled;
        } else if (key == "changes") {
            show_changes_ = enabled;
        }
    }
}

void App::save_settings() {
    const std::filesystem::path path = settings_path();
    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return;
    }
    output << "follow=" << (follow_ ? 1 : 0) << '\n';
    output << "changes=" << (show_changes_ ? 1 : 0) << '\n';
}

void App::update_snapshot_history() {
    previous_ = session_.snapshot();
    have_previous_ = false;
}

void App::open(const std::filesystem::path& path) {
    std::string error;
    const std::string extension = extension_of(path);
    bool ok = false;
    std::vector<AsmDiagnostic> diagnostics;
    if (extension == ".asm" || extension == ".s") {
        ok = session_.assemble(path, error);
        diagnostics = session_.diagnostics();
    } else if (extension == ".k8") {
        ok = session_.load_image(path, error);
    } else if (extension == ".bin") {
        std::vector<std::uint8_t> bytes;
        if (!read_file_bytes(path, bytes, error)) {
            ok = false;
        } else if (bytes.size() > 256) {
            error = "raw image is larger than 256 bytes";
        } else {
            const ImageLoadResult loaded = read_bin(bytes);
            if (!loaded.ok) {
                error = loaded.error;
            } else {
                ok = session_.load_image(loaded.image, path, error);
            }
        }
    } else {
        error = "unsupported file type";
    }

    window_.set_timer(8, false);
    diagnostics_ = std::move(diagnostics);
    errors_open_ = !ok && !diagnostics_.empty();
    if (!ok) {
        message_ = error;
        window_.invalidate();
        return;
    }
    current_path_ = path;
    message_ = "loaded " + path.filename().string();
    window_.set_title("KR8 — " + path.filename().string());
    source_scroll_ = 0;
    asm_scroll_ = 0;
    symbols_scroll_ = 0;
    stack_scroll_ = 0;
    console_scroll_ = 0;
    console_focus_ = false;
    console_field_.focused = false;
    follow_ = true;
    update_snapshot_history();
    layout();
    window_.invalidate();
}

void App::assemble_current() {
    if (current_path_.empty() || !is_source_extension(current_path_)) {
        message_ = "open an .asm file to assemble";
        window_.invalidate();
        return;
    }
    std::string error;
    window_.set_timer(8, false);
    if (session_.assemble(current_path_, error)) {
        message_ = "assembled " + current_path_.filename().string();
        diagnostics_.clear();
        errors_open_ = false;
        follow_ = true;
        update_snapshot_history();
    } else {
        message_ = error;
        diagnostics_ = session_.diagnostics();
        errors_open_ = !diagnostics_.empty();
    }
    layout();
    window_.invalidate();
}

void App::start_run() {
    if (!session_.loaded()) {
        message_ = "load a program first";
        window_.invalidate();
        return;
    }
    session_.start_run();
    message_ = "running";
    follow_ = true;
    window_.set_timer(8, true);
    window_.invalidate();
}

void App::stop_run(std::string reason) {
    session_.stop_run(std::move(reason));
    message_ = session_.message();
    window_.set_timer(8, false);
    window_.invalidate();
}

void App::on_timer() {
    if (!session_.loaded()) {
        window_.set_timer(8, false);
        return;
    }
    previous_ = session_.snapshot();
    have_previous_ = true;
    const RunStop stop = session_.run_slice(50'000);
    message_ = session_.message();
    if (stop != RunStop::Budget) {
        window_.set_timer(8, false);
    }
    window_.invalidate();
}

void App::step_line(StepMode mode) {
    if (session_.running()) {
        session_.stop_run("paused");
    }
    window_.set_timer(8, false);
    previous_ = session_.snapshot();
    have_previous_ = true;
    session_.step_line(mode);
    message_ = session_.message();
    follow_ = true;
    window_.invalidate();
}

void App::step_instruction(int mode) {
    if (session_.running()) {
        session_.stop_run("paused");
    }
    window_.set_timer(8, false);
    previous_ = session_.snapshot();
    have_previous_ = true;
    if (mode == 0) {
        session_.step_instruction_over();
    } else if (mode == 1) {
        session_.step_instruction();
    } else {
        session_.step_instruction_out();
    }
    message_ = session_.message();
    follow_ = true;
    window_.invalidate();
}

void App::toggle_source_breakpoint(int line) {
    if (line <= 0) {
        return;
    }
    session_.toggle_line_breakpoint(line);
    message_ = session_.message();
    window_.invalidate();
}

void App::toggle_asm_breakpoint(std::uint8_t address) {
    session_.toggle_address_breakpoint(address);
    message_ = session_.message();
    window_.invalidate();
}

void App::handle_mouse_down(const MouseDown& down) {
    const Point point = down.position;
    if (help_open_) {
        help_open_ = false;
        window_.invalidate();
        return;
    }
    if (settings_open_) {
        const int row = settings_row_at(point);
        if (row == 0) {
            follow_ = !follow_;
            save_settings();
        } else if (row == 1) {
            show_changes_ = !show_changes_;
            save_settings();
        } else if (row == 3 || row < 0) {
            settings_open_ = false;
        }
        window_.invalidate();
        return;
    }
    if (errors_open_) {
        const int row = error_row_at(point);
        if (row >= 0 && row < static_cast<int>(diagnostics_.size())) {
            const int line =
                diagnostics_[static_cast<std::size_t>(row)].line;
            if (line > 0) {
                source_scroll_ = std::max(0, line - 3);
                follow_ = false;
            }
        }
        errors_open_ = false;
        window_.invalidate();
        return;
    }
    if (editing_register_ >= 0 || editing_memory_ >= 0) {
        cancel_edit();
    }

    if (down.button == MouseButton::Right) {
        const int symbol = symbol_row_at(point);
        if (symbol >= 0) {
            const auto& symbols = session_.symbols();
            if (symbol < static_cast<int>(symbols.size()) &&
                symbols[static_cast<std::size_t>(symbol)].kind !=
                    SymbolKind::Const) {
                const std::int32_t value =
                    symbols[static_cast<std::size_t>(symbol)].value;
                if (value >= 0 && value <= 255) {
                    toggle_asm_breakpoint(
                        static_cast<std::uint8_t>(value));
                    return;
                }
            }
        }
    }

    const int button = hit_button(point);
    if (button >= 0) {
        activate(buttons_[static_cast<std::size_t>(button)].action);
        return;
    }
    if (in_source_gutter(point)) {
        toggle_source_breakpoint(source_line_at(point.y));
        return;
    }
    if (in_asm_gutter(point)) {
        const int row = asm_row_at(point.y);
        if (row >= 0) {
            toggle_asm_breakpoint(
                session_.disassembly()[static_cast<std::size_t>(row)]
                    .address);
        }
        return;
    }
    for (int i = 0; i < 4; ++i) {
        if (register_rects_[i].contains(point)) {
            start_register_edit(i);
            return;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (flag_rects_[i].contains(point)) {
            const std::uint8_t mask = static_cast<std::uint8_t>(1u << i);
            session_.set_flag(i, (session_.snapshot().flags & mask) == 0);
            message_ = "flag toggled";
            window_.invalidate();
            return;
        }
    }
    const int cell = memory_cell_at(point);
    if (cell >= 0) {
        start_memory_edit(memory_cells_[static_cast<std::size_t>(cell)]
                              .address);
        return;
    }
    const int symbol = symbol_row_at(point);
    if (symbol >= 0) {
        const auto& symbols = session_.symbols();
        if (symbol < static_cast<int>(symbols.size())) {
            const Symbol& entry = symbols[static_cast<std::size_t>(symbol)];
            if (entry.kind == SymbolKind::Code && entry.value >= 0 &&
                entry.value <= 255) {
                session_.set_register(2,
                                      static_cast<std::uint8_t>(entry.value));
                message_ = "PC set to " + entry.name;
                follow_ = true;
            } else if (entry.kind == SymbolKind::Data) {
                selected_memory_address_ = entry.value;
                message_ = "selected " + entry.name;
            } else {
                message_ = entry.name + " = " +
                           std::to_string(entry.value);
            }
            window_.invalidate();
        }
        return;
    }
    const int frame = stack_row_at(point);
    if (frame >= 0) {
        const Snapshot snapshot = session_.snapshot();
        if (frame < static_cast<int>(snapshot.frames.size())) {
            session_.set_register(
                2, snapshot.frames[static_cast<std::size_t>(frame)].pc);
            message_ = "PC set to frame " + std::to_string(frame);
            follow_ = true;
            window_.invalidate();
        }
        return;
    }
    if (console_input_.contains(point)) {
        console_focus_ = true;
        console_field_.focused = true;
        text_field_mouse_down(console_field_, console_input_, point.x, false);
        window_.invalidate();
        return;
    }
    if (console_pane_.contains(point)) {
        console_focus_ = true;
        console_field_.focused = true;
        window_.invalidate();
        return;
    }
    console_focus_ = false;
    console_field_.focused = false;
    window_.invalidate();
}

void App::handle_mouse_move(const MouseMove& move) {
    mouse_ = move.position;
    const int hover = hit_button(mouse_);
    const int asm_hover =
        disasm_text_.contains(mouse_) ? asm_row_at(mouse_.y) : -1;
    const int memory_hover =
        memory_text_.contains(mouse_) ? memory_cell_at(mouse_) : -1;
    const bool changed = hover != hover_button_ ||
                         asm_hover != hover_asm_ ||
                         memory_hover != hover_memory_;
    hover_button_ = hover;
    hover_asm_ = asm_hover;
    hover_memory_ = memory_hover;
    if (changed) {
        window_.set_cursor(hover >= 0 ? CursorKind::Hand
                                      : CursorKind::Arrow);
        window_.invalidate();
    }
}

void App::handle_wheel(const MouseWheel& wheel) {
    const int amount = wheel.delta > 0 ? -3 : 3;
    if (source_text_.contains(wheel.position)) {
        source_scroll_ = std::max(0, source_scroll_ + amount);
        follow_ = false;
    } else if (disasm_text_.contains(wheel.position)) {
        asm_scroll_ = std::max(0, asm_scroll_ + amount);
        follow_ = false;
    } else if (symbols_text_.contains(wheel.position)) {
        symbols_scroll_ = std::max(0, symbols_scroll_ + amount);
    } else if (stack_text_.contains(wheel.position)) {
        stack_scroll_ = std::max(0, stack_scroll_ + amount);
    } else if (console_text_.contains(wheel.position)) {
        console_scroll_ = std::max(0, console_scroll_ - amount);
    }
    window_.invalidate();
}

void App::handle_key(const KeyDown& key) {
    if (key.key == Key::F1) {
        help_open_ = !help_open_;
        window_.invalidate();
        return;
    }
    if (help_open_) {
        if (key.key == Key::Escape) {
            help_open_ = false;
            window_.invalidate();
        }
        return;
    }
    if (settings_open_) {
        if (key.key == Key::Escape) {
            settings_open_ = false;
            window_.invalidate();
        }
        return;
    }
    if (errors_open_) {
        if (key.key == Key::Escape) {
            errors_open_ = false;
            window_.invalidate();
        }
        return;
    }
    if (editing_register_ >= 0 || editing_memory_ >= 0) {
        if (key.key == Key::Escape) {
            cancel_edit();
        } else if (key.key == Key::Enter) {
            commit_edit();
        } else if (key.key == Key::Backspace && !edit_text_.empty()) {
            edit_text_.pop_back();
            window_.invalidate();
        }
        return;
    }
    if (console_focus_) {
        if (key.key == Key::Escape) {
            console_focus_ = false;
            console_field_.focused = false;
            window_.invalidate();
            return;
        }
        if (key.key == Key::Enter) {
            for (const char value : console_field_.text) {
                session_.push_input(static_cast<std::uint8_t>(value));
            }
            session_.push_input(static_cast<std::uint8_t>('\n'));
            console_field_.text.clear();
            console_field_.cursor = 0;
            console_field_.anchor = 0;
            console_field_.scroll = 0;
            message_ = "input queued";
            window_.invalidate();
            return;
        }
        if (text_field_key(console_field_, key, window_)) {
            window_.invalidate();
        }
        return;
    }
    if (key.ctrl && key.key == Key::O) {
        open_file_dialog();
        return;
    }
    if (key.ctrl && key.key == Key::R) {
        assemble_current();
        return;
    }
    switch (key.key) {
        case Key::F5: start_run(); break;
        case Key::F6: stop_run("paused"); break;
        case Key::F7: activate(Action::Restart); break;
        case Key::F8: assemble_current(); break;
        case Key::F10: step_line(StepMode::LineOver); break;
        case Key::F11: step_instruction(1); break;
        case Key::Escape:
            message_.clear();
            window_.invalidate();
            break;
        default: break;
    }
}

void App::handle_text_input(std::string_view text) {
    if (help_open_ || settings_open_ || errors_open_) {
        return;
    }
    if (editing_register_ >= 0 || editing_memory_ >= 0) {
        for (const char value : text) {
            if (std::isxdigit(static_cast<unsigned char>(value)) != 0 &&
                edit_text_.size() < 2) {
                edit_text_.push_back(value);
            }
        }
        window_.invalidate();
        return;
    }
    if (console_focus_) {
        std::string filtered;
        for (const char value : text) {
            if (value != '\r' && value != '\n') {
                filtered.push_back(value);
            }
        }
        if (!filtered.empty() && text_field_text(console_field_, filtered)) {
            window_.invalidate();
        }
    }
}

void App::start_register_edit(int index) {
    editing_register_ = index;
    editing_memory_ = -1;
    edit_text_.clear();
    console_focus_ = false;
    console_field_.focused = false;
    message_ = "edit register; Enter applies, Esc cancels";
    window_.invalidate();
}

void App::start_memory_edit(std::uint8_t address) {
    editing_memory_ = static_cast<int>(address);
    editing_memory_address_ = address;
    editing_register_ = -1;
    edit_text_.clear();
    console_focus_ = false;
    console_field_.focused = false;
    message_ = "edit byte; Enter applies, Esc cancels";
    window_.invalidate();
}

void App::commit_edit() {
    previous_ = session_.snapshot();
    have_previous_ = true;
    if (edit_text_.empty()) {
        cancel_edit();
        return;
    }
    try {
        const unsigned long value = std::stoul(edit_text_, nullptr, 16);
        if (value > 255) {
            message_ = "value must be 00..FF";
            window_.invalidate();
            return;
        }
        if (editing_register_ >= 0) {
            session_.set_register(editing_register_,
                                  static_cast<std::uint8_t>(value));
        } else if (editing_memory_ >= 0) {
            session_.write8(editing_memory_address_,
                            static_cast<std::uint8_t>(value));
        }
        message_ = "edit applied";
    } catch (const std::exception&) {
        message_ = "invalid hex value";
    }
    editing_register_ = -1;
    editing_memory_ = -1;
    edit_text_.clear();
    window_.invalidate();
}

void App::cancel_edit() {
    editing_register_ = -1;
    editing_memory_ = -1;
    edit_text_.clear();
    message_ = "edit cancelled";
    window_.invalidate();
}

void App::event(const Event& event) {
    std::visit(
        [this](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, MouseDown>) {
                handle_mouse_down(value);
            } else if constexpr (std::is_same_v<T, MouseMove>) {
                handle_mouse_move(value);
            } else if constexpr (std::is_same_v<T, MouseWheel>) {
                handle_wheel(value);
            } else if constexpr (std::is_same_v<T, KeyDown>) {
                handle_key(value);
            } else if constexpr (std::is_same_v<T, TextInput>) {
                handle_text_input(value.utf8);
            } else if constexpr (std::is_same_v<T, Resize>) {
                resize(value.size);
                window_.invalidate();
            } else if constexpr (std::is_same_v<T, Timer>) {
                on_timer();
            } else if constexpr (std::is_same_v<T, MouseLeave>) {
                hover_button_ = -1;
                window_.invalidate();
            }
        },
        event);
}

}  // namespace kr8::ui
