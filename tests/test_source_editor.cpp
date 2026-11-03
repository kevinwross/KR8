#include <string>

#include "harness.hpp"
#include "kr8/ui/source_editor.hpp"

using namespace kr8::ui;

TEST(source_editor_set_text_round_trips) {
    SourceEditor editor;
    editor.set_text("one\ntwo\n");
    CHECK_EQ(editor.line_count(), 3);
    CHECK_EQ(editor.text(), std::string("one\ntwo\n"));
    CHECK_EQ(editor.cursor_line(), 0);
    CHECK_EQ(editor.cursor_column(), 0);
}

TEST(source_editor_insert_and_newline) {
    SourceEditor editor;
    editor.set_text("start:\n    MOV A, #1");
    editor.set_cursor(0, 6);
    editor.enter();
    CHECK_EQ(editor.text(), std::string("start:\n    \n    MOV A, #1"));
    CHECK_EQ(editor.cursor_line(), 1);
    CHECK_EQ(editor.cursor_column(), 4);
}

TEST(source_editor_selection_and_delete) {
    SourceEditor editor;
    editor.set_text("MOV A, #1\nHLT");
    editor.set_cursor(0, 0);
    editor.set_cursor(0, 3, true);
    CHECK(editor.has_selection());
    CHECK_EQ(editor.selected_text(), std::string("MOV"));
    editor.backspace();
    CHECK_EQ(editor.text(), std::string(" A, #1\nHLT"));
}

TEST(source_editor_multiline_selection) {
    SourceEditor editor;
    editor.set_text("abc\ndef\nghi");
    editor.set_cursor(0, 1);
    editor.set_cursor(2, 1, true);
    CHECK_EQ(editor.selected_text(), std::string("bc\ndef\ng"));
    editor.erase_selection();
    CHECK_EQ(editor.text(), std::string("ahi"));
}

TEST(source_editor_undo_redo) {
    SourceEditor editor;
    editor.set_text("abc");
    editor.set_cursor(0, 3);
    editor.insert_text("d");
    CHECK_EQ(editor.text(), std::string("abcd"));
    CHECK(editor.undo());
    CHECK_EQ(editor.text(), std::string("abc"));
    CHECK(editor.redo());
    CHECK_EQ(editor.text(), std::string("abcd"));
}

TEST(source_editor_vertical_movement_keeps_column) {
    SourceEditor editor;
    editor.set_text("long line\nx\nmedium");
    editor.set_cursor(0, 7);
    editor.move_down();
    CHECK_EQ(editor.cursor_line(), 1);
    CHECK_EQ(editor.cursor_column(), 1);
    editor.move_down();
    CHECK_EQ(editor.cursor_line(), 2);
    CHECK_EQ(editor.cursor_column(), 6);
}

TEST(source_editor_word_movement) {
    SourceEditor editor;
    editor.set_text("MOV A, #msg ; comment");
    editor.set_cursor(0, 0);
    editor.move_word_right();
    CHECK_EQ(editor.cursor_column(), 3);
    editor.move_word_right();
    CHECK_EQ(editor.cursor_column(), 5);
    editor.move_word_left();
    CHECK_EQ(editor.cursor_column(), 4);
}

TEST(source_editor_backspace_joins_lines) {
    SourceEditor editor;
    editor.set_text("abc\ndef");
    editor.set_cursor(1, 0);
    editor.backspace();
    CHECK_EQ(editor.text(), std::string("abcdef"));
    CHECK_EQ(editor.cursor_line(), 0);
    CHECK_EQ(editor.cursor_column(), 3);
}

TEST(source_editor_delete_joins_lines) {
    SourceEditor editor;
    editor.set_text("abc\ndef");
    editor.set_cursor(0, 3);
    editor.delete_forward();
    CHECK_EQ(editor.text(), std::string("abcdef"));
    CHECK_EQ(editor.cursor_line(), 0);
    CHECK_EQ(editor.cursor_column(), 3);
}
