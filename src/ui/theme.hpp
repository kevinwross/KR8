#pragma once

#include "kr8/ui/geometry.hpp"

namespace kr8::ui {

constexpr Color kBg{22, 24, 29, 255};
constexpr Color kPanel{29, 31, 38, 255};
constexpr Color kPanelAlt{35, 38, 47, 255};
constexpr Color kBorder{52, 56, 68, 255};
constexpr Color kText{214, 218, 226, 255};
constexpr Color kTextDim{118, 124, 140, 255};
constexpr Color kComment{104, 112, 128, 255};
constexpr Color kInstruction{126, 170, 255, 255};
constexpr Color kDirective{190, 150, 255, 255};
constexpr Color kNumber{235, 185, 95, 255};
constexpr Color kString{130, 200, 150, 255};
constexpr Color kLabel{170, 200, 255, 255};
constexpr Color kRegister{110, 205, 210, 255};
constexpr Color kSelection{47, 66, 96, 255};
constexpr Color kAccent{82, 156, 255, 255};
constexpr Color kAccentDim{44, 74, 120, 255};
constexpr Color kHighlight{38, 64, 110, 255};
constexpr Color kStmt{31, 43, 58, 255};
constexpr Color kRed{230, 90, 90, 255};
constexpr Color kGreen{130, 200, 150, 255};
constexpr Color kAmber{235, 185, 95, 255};
constexpr Color kButton{52, 56, 68, 255};
constexpr Color kButtonHover{68, 74, 90, 255};
constexpr Color kButtonOff{38, 41, 50, 255};

constexpr int kPad = 8;
constexpr int kTopBar = 36;
constexpr int kHeader = 30;
constexpr int kStatus = 24;
constexpr int kSourceGutter = 58;
constexpr int kAsmGutter = 18;
constexpr int kRowHeight = 20;
constexpr int kRegisterHeight = 88;

}  // namespace kr8::ui
