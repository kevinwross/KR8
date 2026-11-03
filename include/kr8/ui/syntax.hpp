#pragma once

#include <string_view>
#include <vector>

#include "kr8/ui/geometry.hpp"

namespace kr8::ui {

struct SourceSpan {
    int start = 0;
    int length = 0;
    Color color;
};

/// Tokenise one assembly line for syntax colouring. Spans cover the entire
/// line and are ordered left to right.
std::vector<SourceSpan> highlight_source_line(std::string_view line);

}  // namespace kr8::ui
