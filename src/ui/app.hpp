#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "kr8/session.hpp"
#include "kr8/ui/canvas.hpp"
#include "kr8/ui/input.hpp"
#include "kr8/ui/window.hpp"
#include "text_field.hpp"

namespace kr8::ui {

class App final : public Application {
public:
    App(Window& window, Session& session);
    ~App() override = default;

    void paint(Canvas& canvas) override;
    void event(const Event& event) override;
    void resize(Size size) override;
    void open(const std::filesystem::path& path);

private:
    enum class Action {
        None,
        Load,
        Assemble,
        Run,
        Break,
        Restart,
        ResetStats,
        Settings,
        CStepOver,
        CStepIn,
        CStepOut,
        AStepOver,
        AStepIn,
        AStepOut,
    };

    struct Button {
        Rect rect;
        std::string label;
        Action action = Action::None;
        bool enabled = true;
        bool primary = false;
        int group = 0;
    };

    struct AsmRow {
        int index = 0;
        std::uint8_t address = 0;
        Rect rect;
        Rect gutter;
    };

    struct MemoryCell {
        Rect rect;
        std::uint8_t address = 0;
    };

    struct HitRow {
        Rect rect;
        int index = 0;
    };

    void layout();
    void update_buttons();
    void draw_top(Canvas& canvas, const Snapshot& snapshot);
    void draw_source(Canvas& canvas, const Snapshot& snapshot);
    void draw_disassembly(Canvas& canvas, const Snapshot& snapshot);
    void draw_registers(Canvas& canvas, const Snapshot& snapshot);
    void draw_memory(Canvas& canvas, const Snapshot& snapshot);
    void draw_symbols(Canvas& canvas, const Snapshot& snapshot);
    void draw_stack(Canvas& canvas, const Snapshot& snapshot);
    void draw_console(Canvas& canvas, const Snapshot& snapshot);
    void draw_status(Canvas& canvas, const Snapshot& snapshot);
    void draw_help(Canvas& canvas);
    void draw_tooltip(Canvas& canvas, const Snapshot& snapshot);
    void draw_errors(Canvas& canvas);
    void draw_settings(Canvas& canvas);
    void draw_pane_header(Canvas& canvas, const Rect& pane,
                          std::string_view title, int group);
    void draw_button(Canvas& canvas, const Button& button);

    void activate(Action action);
    void open_file_dialog();
    void open_settings();
    void assemble_current();
    void start_run();
    void stop_run(std::string reason);
    void on_timer();
    void step_line(StepMode mode);
    void step_instruction(int mode);
    void toggle_source_breakpoint(int line);
    void toggle_asm_breakpoint(std::uint8_t address);
    void update_snapshot_history();
    void load_settings();
    void save_settings();

    void handle_mouse_down(const MouseDown& down);
    void handle_mouse_move(const MouseMove& move);
    void handle_wheel(const MouseWheel& wheel);
    void handle_key(const KeyDown& key);
    void handle_text_input(std::string_view text);
    void start_register_edit(int index);
    void start_memory_edit(std::uint8_t address);
    void commit_edit();
    void cancel_edit();

    int hit_button(Point point) const;
    bool in_source_gutter(Point point) const;
    bool in_asm_gutter(Point point) const;
    int source_line_at(int y) const;
    int asm_row_at(int y) const;
    int memory_cell_at(Point point) const;
    int symbol_row_at(Point point) const;
    int stack_row_at(Point point) const;
    int error_row_at(Point point) const;
    int settings_row_at(Point point) const;
    std::string status_text(const Snapshot& snapshot) const;

    Window& window_;
    Session& session_;
    Size size_{1280, 820};

    Rect top_bar_{};
    Rect source_pane_{};
    Rect source_text_{};
    Rect disasm_pane_{};
    Rect disasm_text_{};
    Rect register_pane_{};
    Rect memory_pane_{};
    Rect memory_text_{};
    Rect symbols_pane_{};
    Rect symbols_text_{};
    Rect stack_pane_{};
    Rect stack_text_{};
    Rect console_pane_{};
    Rect console_text_{};
    Rect console_input_{};
    Rect status_{};

    std::vector<Button> buttons_;
    std::vector<AsmRow> asm_rows_;
    std::vector<MemoryCell> memory_cells_;
    std::vector<HitRow> symbol_rows_;
    std::vector<HitRow> stack_rows_;
    std::vector<HitRow> error_rows_;
    std::vector<HitRow> settings_rows_;
    std::vector<std::string> help_lines_;
    std::vector<AsmDiagnostic> diagnostics_;
    Snapshot previous_;
    bool have_previous_ = false;

    std::filesystem::path current_path_;
    std::string message_;
    std::string edit_text_;
    int editing_register_ = -1;
    int editing_memory_ = -1;
    std::uint8_t editing_memory_address_ = 0;
    int selected_memory_address_ = -1;
    bool console_focus_ = false;
    bool help_open_ = false;
    bool errors_open_ = false;
    bool settings_open_ = false;
    bool follow_ = true;
    bool show_changes_ = true;
    int hover_asm_ = -1;
    int hover_memory_ = -1;

    int source_scroll_ = 0;
    int asm_scroll_ = 0;
    int symbols_scroll_ = 0;
    int stack_scroll_ = 0;
    int console_scroll_ = 0;
    int hover_button_ = -1;

    TextField console_field_;
    Rect register_rects_[4]{};
    Rect flag_rects_[4]{};
    Point mouse_{};
};

}  // namespace kr8::ui
