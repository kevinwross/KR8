#include "win32_canvas.hpp"

#include <algorithm>
#include <string_view>

#include "resources/embedded_fonts.hpp"

namespace kr8::ui::win32 {
namespace {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int size =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                            static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                        static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

COLORREF to_color(Color color) {
    return RGB(color.r, color.g, color.b);
}

HFONT make_font(int height, int weight, const wchar_t* face) {
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
}

const wchar_t* g_ui_face = L"Segoe UI";
const wchar_t* g_mono_face = L"Consolas";

void register_embedded_fonts() {
    static bool attempted = false;
    if (attempted) {
        return;
    }
    attempted = true;
    const EmbeddedFonts fonts = embedded_fonts();
    DWORD count = 0;
    HANDLE ui = AddFontMemResourceEx(
        const_cast<std::uint8_t*>(fonts.ui_regular.data()),
        static_cast<DWORD>(fonts.ui_regular.size()), nullptr, &count);
    HANDLE mono = AddFontMemResourceEx(
        const_cast<std::uint8_t*>(fonts.mono_regular.data()),
        static_cast<DWORD>(fonts.mono_regular.size()), nullptr, &count);
    if (ui != nullptr) {
        g_ui_face = L"Inter";
    }
    if (mono != nullptr) {
        g_mono_face = L"JetBrains Mono";
    }
}

}  // namespace

Win32Canvas::Win32Canvas(HDC dc, RECT client) : dc_(dc), client_(client) {
    register_embedded_fonts();
    ui_ = make_font(-14, FW_NORMAL, g_ui_face);
    ui_bold_ = make_font(-14, FW_SEMIBOLD, g_ui_face);
    mono_ = make_font(-15, FW_NORMAL, g_mono_face);
    mono_small_ = make_font(-13, FW_NORMAL, g_mono_face);
}

Win32Canvas::~Win32Canvas() {
    for (HFONT font_handle : {ui_, ui_bold_, mono_, mono_small_}) {
        if (font_handle != nullptr) {
            DeleteObject(font_handle);
        }
    }
}

HFONT Win32Canvas::font(FontRole role) const {
    switch (role) {
        case FontRole::Ui:
            return ui_;
        case FontRole::UiBold:
            return ui_bold_;
        case FontRole::Mono:
            return mono_;
        case FontRole::MonoSmall:
            return mono_small_;
    }
    return ui_;
}

RECT Win32Canvas::to_native(const Rect& rect) const {
    return RECT{rect.left, rect.top, rect.right, rect.bottom};
}

void Win32Canvas::fill_rect(const Rect& rect, Color color) {
    if (rect.empty()) {
        return;
    }
    HBRUSH brush = CreateSolidBrush(to_color(color));
    RECT native = to_native(rect);
    FillRect(dc_, &native, brush);
    DeleteObject(brush);
}

void Win32Canvas::frame_rect(const Rect& rect, Color color, int thickness) {
    if (rect.empty() || thickness <= 0) {
        return;
    }
    const int t = std::min(thickness, std::min(rect.width(), rect.height()));
    fill_rect(Rect{rect.left, rect.top, rect.right, rect.top + t}, color);
    fill_rect(Rect{rect.left, rect.bottom - t, rect.right, rect.bottom},
              color);
    fill_rect(Rect{rect.left, rect.top, rect.left + t, rect.bottom}, color);
    fill_rect(Rect{rect.right - t, rect.top, rect.right, rect.bottom}, color);
}

void Win32Canvas::text(int x, int y, std::string_view utf8, Color color,
                       FontRole role) {
    const std::wstring wide = widen(utf8);
    if (wide.empty()) {
        return;
    }
    const HGDIOBJ old_font = SelectObject(dc_, font(role));
    SetBkMode(dc_, TRANSPARENT);
    SetTextColor(dc_, to_color(color));
    SetTextAlign(dc_, TA_LEFT | TA_TOP);
    TextOutW(dc_, x, y, wide.c_str(), static_cast<int>(wide.size()));
    SelectObject(dc_, old_font);
}

Size Win32Canvas::measure(std::string_view utf8, FontRole role) {
    const std::wstring wide = widen(utf8);
    if (wide.empty()) {
        return Size{0, line_height(role)};
    }
    const HGDIOBJ old_font = SelectObject(dc_, font(role));
    SIZE size{};
    GetTextExtentPoint32W(dc_, wide.c_str(), static_cast<int>(wide.size()),
                          &size);
    SelectObject(dc_, old_font);
    return Size{static_cast<int>(size.cx), static_cast<int>(size.cy)};
}

int Win32Canvas::line_height(FontRole role) {
    const HGDIOBJ old_font = SelectObject(dc_, font(role));
    TEXTMETRICW metric{};
    GetTextMetricsW(dc_, &metric);
    SelectObject(dc_, old_font);
    return static_cast<int>(metric.tmHeight + metric.tmExternalLeading);
}

void Win32Canvas::push_clip(const Rect& rect) {
    const int saved = SaveDC(dc_);
    IntersectClipRect(dc_, rect.left, rect.top, rect.right, rect.bottom);
    saved_dc_.push_back(saved);
}

void Win32Canvas::pop_clip() {
    if (saved_dc_.empty()) {
        return;
    }
    RestoreDC(dc_, saved_dc_.back());
    saved_dc_.pop_back();
}

}  // namespace kr8::ui::win32
