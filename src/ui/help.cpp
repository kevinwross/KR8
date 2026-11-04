#include "help.hpp"

#include <string>

#include "kr8/isa.hpp"

namespace kr8::ui {

std::vector<std::string> help_lines() {
    std::vector<std::string> lines;
    lines.emplace_back("KR8 quick reference");
    lines.emplace_back("");
    lines.emplace_back("Registers: A, B, PC, SP, FLAGS (Z C N V)");
    lines.emplace_back("RAM: 256 bytes; stack grows down from 0xFF.");
    lines.emplace_back("image_high is the stack floor; a push below it faults.");
    lines.emplace_back("IN is non-blocking: Z=0 when a byte was read, Z=1 when not.");
    lines.emplace_back("");
    lines.emplace_back("Instructions");
    for (const auto& info : kr8::opcode_table()) {
        lines.emplace_back(std::string(info.mnemonic) + "  -  " +
                           std::string(info.help));
    }
    lines.emplace_back("");
    lines.emplace_back("Directives");
    lines.emplace_back(".org expr          set the location counter");
    lines.emplace_back(".byte expr, ...    emit bytes");
    lines.emplace_back(".word expr, ...    emit little-endian words");
    lines.emplace_back(".ascii \"text\"     emit text");
    lines.emplace_back(".asciz \"text\"     emit text and a NUL");
    lines.emplace_back(".space n           reserve n zero bytes");
    lines.emplace_back(".equ NAME, expr    define a constant");
    lines.emplace_back(".entry expr        set the entry address");
    lines.emplace_back("");
    lines.emplace_back("Keys");
    lines.emplace_back("F5 run    F6 break    F7 restart    F8 assemble");
    lines.emplace_back("F10 step line    F11 step instruction    F1 help");
    lines.emplace_back("Ctrl+O load    Ctrl+R assemble    Esc close");
    lines.emplace_back("Console: hover the pane and type; keys go to IN.");
    lines.emplace_back("Enter sends a newline; Ctrl+V pastes; no input box.");
    lines.emplace_back("");
    lines.emplace_back("Editing (Source Edit button)");
    lines.emplace_back("Ctrl+S save    Ctrl+Z undo    Ctrl+Y redo");
    lines.emplace_back("Ctrl+A select all    Ctrl+C/X/V clipboard");
    lines.emplace_back("F8 assemble and reload    Esc leave edit mode");
    return lines;
}

}  // namespace kr8::ui
