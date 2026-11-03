#include "kr8/ui/source_editor.hpp"

#include <algorithm>
#include <cctype>

namespace kr8::ui {
namespace {

bool is_word_char(char value) {
    const auto byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_' || value == '.';
}

bool position_less(int line_a, int column_a, int line_b, int column_b) {
    return line_a < line_b || (line_a == line_b && column_a < column_b);
}

}  // namespace

void SourceEditor::set_text(std::string_view text) {
    lines_.clear();
    std::string line;
    for (const char value : text) {
        if (value == '\r') {
            continue;
        }
        if (value == '\n') {
            lines_.push_back(line);
            line.clear();
        } else {
            line.push_back(value);
        }
    }
    lines_.push_back(line);
    cursor_line_ = 0;
    cursor_column_ = 0;
    anchor_line_ = 0;
    anchor_column_ = 0;
    preferred_column_ = 0;
    undo_.clear();
    redo_.clear();
}

std::string SourceEditor::text() const {
    std::string result;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (i != 0) {
            result.push_back('\n');
        }
        result += lines_[i];
    }
    return result;
}

int SourceEditor::line_count() const noexcept {
    return static_cast<int>(lines_.size());
}

bool SourceEditor::has_selection() const noexcept {
    return cursor_line_ != anchor_line_ || cursor_column_ != anchor_column_;
}

void SourceEditor::clamp_cursor() {
    if (lines_.empty()) {
        lines_.emplace_back();
    }
    cursor_line_ = std::clamp(cursor_line_, 0, line_count() - 1);
    cursor_column_ =
        std::clamp(cursor_column_, 0,
                   static_cast<int>(lines_[static_cast<std::size_t>(
                       cursor_line_)].size()));
    anchor_line_ = std::clamp(anchor_line_, 0, line_count() - 1);
    anchor_column_ =
        std::clamp(anchor_column_, 0,
                   static_cast<int>(lines_[static_cast<std::size_t>(
                       anchor_line_)].size()));
    preferred_column_ = std::max(0, preferred_column_);
}

void SourceEditor::set_cursor_internal(int line, int column, bool extend,
                                       bool keep_preferred) {
    cursor_line_ = std::clamp(line, 0, line_count() - 1);
    cursor_column_ =
        std::clamp(column, 0,
                   static_cast<int>(lines_[static_cast<std::size_t>(
                       cursor_line_)].size()));
    if (!extend) {
        anchor_line_ = cursor_line_;
        anchor_column_ = cursor_column_;
    }
    if (!keep_preferred) {
        preferred_column_ = cursor_column_;
    }
}

void SourceEditor::set_cursor(int line, int column, bool extend) {
    set_cursor_internal(line, column, extend, false);
}

void SourceEditor::select_all() {
    anchor_line_ = 0;
    anchor_column_ = 0;
    cursor_line_ = line_count() - 1;
    cursor_column_ = static_cast<int>(
        lines_[static_cast<std::size_t>(cursor_line_)].size());
    preferred_column_ = cursor_column_;
}

std::string SourceEditor::selected_text() const {
    if (!has_selection()) {
        return {};
    }
    int start_line = cursor_line_;
    int start_column = cursor_column_;
    int end_line = anchor_line_;
    int end_column = anchor_column_;
    if (position_less(end_line, end_column, start_line, start_column)) {
        std::swap(start_line, end_line);
        std::swap(start_column, end_column);
    }
    if (start_line == end_line) {
        return lines_[static_cast<std::size_t>(start_line)].substr(
            static_cast<std::size_t>(start_column),
            static_cast<std::size_t>(end_column - start_column));
    }
    std::string result =
        lines_[static_cast<std::size_t>(start_line)].substr(
            static_cast<std::size_t>(start_column));
    for (int line = start_line + 1; line <= end_line; ++line) {
        result.push_back('\n');
        const std::string& text = lines_[static_cast<std::size_t>(line)];
        result += line == end_line
                      ? text.substr(0, static_cast<std::size_t>(end_column))
                      : text;
    }
    return result;
}

void SourceEditor::push_undo() {
    State state;
    state.lines = lines_;
    state.cursor_line = cursor_line_;
    state.cursor_column = cursor_column_;
    state.anchor_line = anchor_line_;
    state.anchor_column = anchor_column_;
    undo_.push_back(std::move(state));
    if (undo_.size() > 256) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

void SourceEditor::erase_selection_internal() {
    if (!has_selection()) {
        return;
    }
    int start_line = cursor_line_;
    int start_column = cursor_column_;
    int end_line = anchor_line_;
    int end_column = anchor_column_;
    if (position_less(end_line, end_column, start_line, start_column)) {
        std::swap(start_line, end_line);
        std::swap(start_column, end_column);
    }
    if (start_line == end_line) {
        lines_[static_cast<std::size_t>(start_line)].erase(
            static_cast<std::size_t>(start_column),
            static_cast<std::size_t>(end_column - start_column));
    } else {
        const std::string tail =
            lines_[static_cast<std::size_t>(end_line)].substr(
                static_cast<std::size_t>(end_column));
        lines_[static_cast<std::size_t>(start_line)].erase(
            static_cast<std::size_t>(start_column));
        lines_[static_cast<std::size_t>(start_line)] += tail;
        lines_.erase(lines_.begin() + start_line + 1,
                     lines_.begin() + end_line + 1);
    }
    cursor_line_ = start_line;
    cursor_column_ = start_column;
    anchor_line_ = cursor_line_;
    anchor_column_ = cursor_column_;
    preferred_column_ = cursor_column_;
}

void SourceEditor::insert_text(std::string_view text) {
    if (text.empty()) {
        return;
    }
    push_undo();
    erase_selection_internal();
    for (const char value : text) {
        if (value == '\r') {
            continue;
        }
        if (value == '\n') {
            std::string& line = lines_[static_cast<std::size_t>(cursor_line_)];
            std::string tail =
                line.substr(static_cast<std::size_t>(cursor_column_));
            line.erase(static_cast<std::size_t>(cursor_column_));
            lines_.insert(lines_.begin() + cursor_line_ + 1,
                          std::move(tail));
            ++cursor_line_;
            cursor_column_ = 0;
        } else {
            lines_[static_cast<std::size_t>(cursor_line_)].insert(
                static_cast<std::size_t>(cursor_column_), 1, value);
            ++cursor_column_;
        }
    }
    anchor_line_ = cursor_line_;
    anchor_column_ = cursor_column_;
    preferred_column_ = cursor_column_;
}

void SourceEditor::erase_selection() {
    if (!has_selection()) {
        return;
    }
    push_undo();
    erase_selection_internal();
}

std::string SourceEditor::indentation_for_new_line() const {
    const std::string& line =
        lines_[static_cast<std::size_t>(cursor_line_)];
    const std::size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return line;
    }
    std::string indent = line.substr(0, first);
    const std::string trimmed = line.substr(first);
    if (cursor_column_ == static_cast<int>(line.size()) &&
        !trimmed.empty() && trimmed.front() != ';' &&
        trimmed.back() == ':') {
        indent += "    ";
    }
    return indent;
}

void SourceEditor::enter() {
    insert_text("\n" + indentation_for_new_line());
}

void SourceEditor::backspace() {
    push_undo();
    if (has_selection()) {
        erase_selection_internal();
        return;
    }
    if (cursor_column_ > 0) {
        lines_[static_cast<std::size_t>(cursor_line_)].erase(
            static_cast<std::size_t>(cursor_column_ - 1), 1);
        --cursor_column_;
    } else if (cursor_line_ > 0) {
        const int previous_length = static_cast<int>(
            lines_[static_cast<std::size_t>(cursor_line_ - 1)].size());
        lines_[static_cast<std::size_t>(cursor_line_ - 1)] +=
            lines_[static_cast<std::size_t>(cursor_line_)];
        lines_.erase(lines_.begin() + cursor_line_);
        --cursor_line_;
        cursor_column_ = previous_length;
    }
    anchor_line_ = cursor_line_;
    anchor_column_ = cursor_column_;
    preferred_column_ = cursor_column_;
}

void SourceEditor::delete_forward() {
    push_undo();
    if (has_selection()) {
        erase_selection_internal();
        return;
    }
    std::string& line = lines_[static_cast<std::size_t>(cursor_line_)];
    if (cursor_column_ < static_cast<int>(line.size())) {
        line.erase(static_cast<std::size_t>(cursor_column_), 1);
    } else if (cursor_line_ + 1 < line_count()) {
        line += lines_[static_cast<std::size_t>(cursor_line_ + 1)];
        lines_.erase(lines_.begin() + cursor_line_ + 1);
    }
    anchor_line_ = cursor_line_;
    anchor_column_ = cursor_column_;
    preferred_column_ = cursor_column_;
}

int SourceEditor::find_word_left(int line, int column) const {
    const std::string& text = lines_[static_cast<std::size_t>(line)];
    while (column > 0 &&
           std::isspace(static_cast<unsigned char>(text[static_cast<std::size_t>(
               column - 1)])) != 0) {
        --column;
    }
    if (column == 0) {
        return 0;
    }
    if (!is_word_char(text[static_cast<std::size_t>(column - 1)])) {
        return column - 1;
    }
    while (column > 0 &&
           is_word_char(text[static_cast<std::size_t>(column - 1)])) {
        --column;
    }
    return column;
}

int SourceEditor::find_word_right(int line, int column) const {
    const std::string& text = lines_[static_cast<std::size_t>(line)];
    const int length = static_cast<int>(text.size());
    while (column < length &&
           std::isspace(static_cast<unsigned char>(
               text[static_cast<std::size_t>(column)])) != 0) {
        ++column;
    }
    if (column >= length) {
        return length;
    }
    if (!is_word_char(text[static_cast<std::size_t>(column)])) {
        return column + 1;
    }
    while (column < length &&
           is_word_char(text[static_cast<std::size_t>(column)])) {
        ++column;
    }
    return column;
}

void SourceEditor::erase_word_backward() {
    if (has_selection()) {
        push_undo();
        erase_selection_internal();
        return;
    }
    if (cursor_column_ == 0 && cursor_line_ > 0) {
        backspace();
        return;
    }
    const int target = find_word_left(cursor_line_, cursor_column_);
    if (target == cursor_column_) {
        return;
    }
    push_undo();
    lines_[static_cast<std::size_t>(cursor_line_)].erase(
        static_cast<std::size_t>(target),
        static_cast<std::size_t>(cursor_column_ - target));
    cursor_column_ = target;
    anchor_line_ = cursor_line_;
    anchor_column_ = cursor_column_;
    preferred_column_ = cursor_column_;
}

void SourceEditor::erase_word_forward() {
    if (has_selection()) {
        push_undo();
        erase_selection_internal();
        return;
    }
    if (cursor_column_ ==
            static_cast<int>(
                lines_[static_cast<std::size_t>(cursor_line_)].size()) &&
        cursor_line_ + 1 < line_count()) {
        delete_forward();
        return;
    }
    const int target = find_word_right(cursor_line_, cursor_column_);
    if (target == cursor_column_) {
        return;
    }
    push_undo();
    lines_[static_cast<std::size_t>(cursor_line_)].erase(
        static_cast<std::size_t>(cursor_column_),
        static_cast<std::size_t>(target - cursor_column_));
    anchor_line_ = cursor_line_;
    anchor_column_ = cursor_column_;
    preferred_column_ = cursor_column_;
}

void SourceEditor::move_left(bool extend) {
    if (has_selection() && !extend) {
        const bool cursor_first =
            position_less(cursor_line_, cursor_column_, anchor_line_,
                          anchor_column_);
        const int line = cursor_first ? cursor_line_ : anchor_line_;
        const int column = cursor_first ? cursor_column_ : anchor_column_;
        set_cursor_internal(line, column, false, false);
        return;
    }
    if (cursor_column_ > 0) {
        set_cursor_internal(cursor_line_, cursor_column_ - 1, extend, false);
    } else if (cursor_line_ > 0) {
        const int previous = cursor_line_ - 1;
        set_cursor_internal(
            previous,
            static_cast<int>(
                lines_[static_cast<std::size_t>(previous)].size()),
            extend, false);
    }
}

void SourceEditor::move_right(bool extend) {
    if (has_selection() && !extend) {
        const bool cursor_first =
            position_less(cursor_line_, cursor_column_, anchor_line_,
                          anchor_column_);
        const int line = cursor_first ? anchor_line_ : cursor_line_;
        const int column = cursor_first ? anchor_column_ : cursor_column_;
        set_cursor_internal(line, column, false, false);
        return;
    }
    const int length = static_cast<int>(
        lines_[static_cast<std::size_t>(cursor_line_)].size());
    if (cursor_column_ < length) {
        set_cursor_internal(cursor_line_, cursor_column_ + 1, extend, false);
    } else if (cursor_line_ + 1 < line_count()) {
        set_cursor_internal(cursor_line_ + 1, 0, extend, false);
    }
}

void SourceEditor::move_up(bool extend) {
    if (cursor_line_ == 0) {
        return;
    }
    const int target = cursor_line_ - 1;
    const int column = std::min(
        preferred_column_,
        static_cast<int>(lines_[static_cast<std::size_t>(target)].size()));
    set_cursor_internal(target, column, extend, true);
}

void SourceEditor::move_down(bool extend) {
    if (cursor_line_ + 1 >= line_count()) {
        return;
    }
    const int target = cursor_line_ + 1;
    const int column = std::min(
        preferred_column_,
        static_cast<int>(lines_[static_cast<std::size_t>(target)].size()));
    set_cursor_internal(target, column, extend, true);
}

void SourceEditor::move_page(int lines, bool extend) {
    const int target = std::clamp(cursor_line_ + lines, 0, line_count() - 1);
    const int column = std::min(
        preferred_column_,
        static_cast<int>(lines_[static_cast<std::size_t>(target)].size()));
    set_cursor_internal(target, column, extend, true);
}

void SourceEditor::move_home(bool extend) {
    set_cursor_internal(cursor_line_, 0, extend, false);
}

void SourceEditor::move_end(bool extend) {
    set_cursor_internal(
        cursor_line_,
        static_cast<int>(
            lines_[static_cast<std::size_t>(cursor_line_)].size()),
        extend, false);
}

void SourceEditor::move_document_start(bool extend) {
    set_cursor_internal(0, 0, extend, false);
}

void SourceEditor::move_document_end(bool extend) {
    const int last = line_count() - 1;
    set_cursor_internal(
        last,
        static_cast<int>(lines_[static_cast<std::size_t>(last)].size()),
        extend, false);
}

void SourceEditor::move_word_left(bool extend) {
    if (cursor_column_ == 0) {
        if (cursor_line_ == 0) {
            return;
        }
        const int previous = cursor_line_ - 1;
        set_cursor_internal(
            previous,
            static_cast<int>(
                lines_[static_cast<std::size_t>(previous)].size()),
            extend, false);
        return;
    }
    set_cursor_internal(
        cursor_line_, find_word_left(cursor_line_, cursor_column_), extend,
        false);
}

void SourceEditor::move_word_right(bool extend) {
    const int length = static_cast<int>(
        lines_[static_cast<std::size_t>(cursor_line_)].size());
    if (cursor_column_ >= length) {
        if (cursor_line_ + 1 >= line_count()) {
            return;
        }
        set_cursor_internal(cursor_line_ + 1, 0, extend, false);
        return;
    }
    set_cursor_internal(
        cursor_line_, find_word_right(cursor_line_, cursor_column_), extend,
        false);
}

void SourceEditor::restore(const State& state) {
    lines_ = state.lines;
    cursor_line_ = state.cursor_line;
    cursor_column_ = state.cursor_column;
    anchor_line_ = state.anchor_line;
    anchor_column_ = state.anchor_column;
    preferred_column_ = cursor_column_;
    clamp_cursor();
}

bool SourceEditor::undo() {
    if (undo_.empty()) {
        return false;
    }
    State current;
    current.lines = lines_;
    current.cursor_line = cursor_line_;
    current.cursor_column = cursor_column_;
    current.anchor_line = anchor_line_;
    current.anchor_column = anchor_column_;
    redo_.push_back(std::move(current));
    const State state = undo_.back();
    undo_.pop_back();
    restore(state);
    return true;
}

bool SourceEditor::redo() {
    if (redo_.empty()) {
        return false;
    }
    State current;
    current.lines = lines_;
    current.cursor_line = cursor_line_;
    current.cursor_column = cursor_column_;
    current.anchor_line = anchor_line_;
    current.anchor_column = anchor_column_;
    undo_.push_back(std::move(current));
    const State state = redo_.back();
    redo_.pop_back();
    restore(state);
    return true;
}

}  // namespace kr8::ui
