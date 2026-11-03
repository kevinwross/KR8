#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace kr8::ui {

/// Portable multiline source buffer used by the debugger's Edit mode.
///
/// Columns are byte offsets into a line; KR8 sources are ASCII assembly. The
/// model owns only text, selection, and undo history, so it is unit-testable
/// without a window or canvas.
class SourceEditor {
public:
    void set_text(std::string_view text);

    [[nodiscard]] std::string text() const;
    [[nodiscard]] int line_count() const noexcept;
    [[nodiscard]] const std::vector<std::string>& lines() const noexcept {
        return lines_;
    }

    [[nodiscard]] int cursor_line() const noexcept { return cursor_line_; }
    [[nodiscard]] int cursor_column() const noexcept { return cursor_column_; }
    [[nodiscard]] int anchor_line() const noexcept { return anchor_line_; }
    [[nodiscard]] int anchor_column() const noexcept {
        return anchor_column_;
    }
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::string selected_text() const;

    void set_cursor(int line, int column, bool extend = false);
    void select_all();

    void insert_text(std::string_view text);
    void erase_selection();
    void enter();
    void backspace();
    void delete_forward();
    void erase_word_backward();
    void erase_word_forward();

    void move_left(bool extend = false);
    void move_right(bool extend = false);
    void move_up(bool extend = false);
    void move_down(bool extend = false);
    void move_page(int lines, bool extend = false);
    void move_home(bool extend = false);
    void move_end(bool extend = false);
    void move_document_start(bool extend = false);
    void move_document_end(bool extend = false);
    void move_word_left(bool extend = false);
    void move_word_right(bool extend = false);

    bool undo();
    bool redo();
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }

private:
    struct State {
        std::vector<std::string> lines;
        int cursor_line = 0;
        int cursor_column = 0;
        int anchor_line = 0;
        int anchor_column = 0;
    };

    void push_undo();
    void erase_selection_internal();
    void restore(const State& state);
    void clamp_cursor();
    void set_cursor_internal(int line, int column, bool extend,
                             bool keep_preferred);
    [[nodiscard]] int find_word_left(int line, int column) const;
    [[nodiscard]] int find_word_right(int line, int column) const;
    [[nodiscard]] std::string indentation_for_new_line() const;

    std::vector<std::string> lines_{std::string()};
    int cursor_line_ = 0;
    int cursor_column_ = 0;
    int anchor_line_ = 0;
    int anchor_column_ = 0;
    int preferred_column_ = 0;
    std::vector<State> undo_;
    std::vector<State> redo_;
};

}  // namespace kr8::ui
