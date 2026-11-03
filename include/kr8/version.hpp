#pragma once

#include <string_view>

#ifndef KR8_VERSION
#define KR8_VERSION "0.0.0"
#endif

namespace kr8 {

inline constexpr std::string_view kVersion = KR8_VERSION;

std::string_view version();

}  // namespace kr8
