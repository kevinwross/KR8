#include "kr8/ui/syntax.hpp"

#include <cctype>
#include <string>
#include <vector>

#include "kr8/isa.hpp"
#include "theme.hpp"

namespace kr8::ui {
namespace {

bool identifier_start(char value) {
    const auto byte = static_cast<unsigned char>(value);
    return std::isalpha(byte) != 0 || value == '_' || value == '.';
}

bool identifier_char(char value) {
    const auto byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_' || value == '.';
}

bool number_start(std::string_view line, std::size_t index) {
    const char value = line[index];
    if (std::isdigit(static_cast<unsigned char>(value)) != 0 ||
        value == '$') {
        return true;
    }
    if (value == '0' && index + 1 < line.size() &&
        (line[index + 1] == 'x' || line[index + 1] == 'X' ||
         line[index + 1] == 'b' || line[index + 1] == 'B')) {
        return true;
    }
    return false;
}

}  // namespace

std::vector<SourceSpan> highlight_source_line(std::string_view line) {
    std::vector<SourceSpan> spans;
    if (line.empty()) {
        return spans;
    }
    std::vector<Color> colors(line.size(), kText);

    bool in_string = false;
    char quote = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char value = line[i];
        if (in_string) {
            colors[i] = kString;
            if (value == '\\' && i + 1 < line.size()) {
                colors[++i] = kString;
            } else if (value == quote) {
                in_string = false;
            }
            continue;
        }
        if (value == '"' || value == '\'') {
            in_string = true;
            quote = value;
            colors[i] = kString;
            continue;
        }
        if (value == ';') {
            for (std::size_t j = i; j < line.size(); ++j) {
                colors[j] = kComment;
            }
            break;
        }
    }

    for (std::size_t i = 0; i < line.size();) {
        if (colors[i] != kText) {
            ++i;
            continue;
        }
        const char value = line[i];
        if (identifier_start(value)) {
            std::size_t end = i + 1;
            while (end < line.size() && identifier_char(line[end])) {
                ++end;
            }
            const std::string token(line.substr(i, end - i));
            std::size_t next = end;
            while (next < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[next])) != 0) {
                ++next;
            }
            Color color = kText;
            if (next < line.size() && line[next] == ':') {
                color = kLabel;
            } else if (!token.empty() && token.front() == '.') {
                color = kDirective;
            } else if (find_mnemonic(token) != nullptr) {
                color = kInstruction;
            } else if (token == "A" || token == "B" || token == "a" ||
                       token == "b") {
                color = kRegister;
            }
            if (color != kText) {
                for (std::size_t j = i; j < end; ++j) {
                    colors[j] = color;
                }
            }
            i = end;
            continue;
        }
        if (number_start(line, i)) {
            std::size_t end = i + 1;
            while (end < line.size() &&
                   std::isalnum(static_cast<unsigned char>(line[end])) != 0) {
                ++end;
            }
            for (std::size_t j = i; j < end; ++j) {
                colors[j] = kNumber;
            }
            i = end;
            continue;
        }
        ++i;
    }

    for (std::size_t i = 0; i < line.size();) {
        const Color color = colors[i];
        std::size_t end = i + 1;
        while (end < line.size() && colors[end] == color) {
            ++end;
        }
        spans.push_back(SourceSpan{static_cast<int>(i),
                                   static_cast<int>(end - i), color});
        i = end;
    }
    return spans;
}

}  // namespace kr8::ui
