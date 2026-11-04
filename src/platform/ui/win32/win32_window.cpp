#include "win32_window.hpp"

#include <commdlg.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "win32_canvas.hpp"

namespace kr8::ui::win32 {
namespace {

constexpr wchar_t kWindowClass[] = L"KR8UiWindow";
constexpr UINT_PTR kTimerId = 1;

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

std::string narrow(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                         static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                        static_cast<int>(wide.size()), result.data(), size,
                        nullptr, nullptr);
    return result;
}

Key map_key(WPARAM key) {
    switch (key) {
        case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_BACK: return Key::Backspace;
        case VK_DELETE: return Key::Delete;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_F1: return Key::F1;
        case VK_F5: return Key::F5;
        case VK_F6: return Key::F6;
        case VK_F7: return Key::F7;
        case VK_F8: return Key::F8;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case 'A': return Key::A;
        case 'C': return Key::C;
        case 'O': return Key::O;
        case 'R': return Key::R;
        case 'S': return Key::S;
        case 'V': return Key::V;
        case 'X': return Key::X;
        case 'Y': return Key::Y;
        case 'Z': return Key::Z;
        default: return Key::Unknown;
    }
}

MouseButton map_button(UINT message) {
    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            return MouseButton::Left;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return MouseButton::Middle;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return MouseButton::Right;
        default:
            return MouseButton::None;
    }
}

}  // namespace

Win32Window::Win32Window(std::wstring title, int width, int height)
    : title_(std::move(title)), width_(width), height_(height) {
    SetProcessDPIAware();
}

Win32Window::~Win32Window() = default;

int Win32Window::run(Application& application) {
    application_ = &application;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &Win32Window::window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kWindowClass;
    RegisterClassExW(&window_class);

    RECT bounds{0, 0, width_, height_};
    const DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&bounds, style, FALSE);

    hwnd_ = CreateWindowExW(
        0, kWindowClass, title_.c_str(), style, CW_USEDEFAULT,
        CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (hwnd_ == nullptr) {
        return 1;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void Win32Window::invalidate() {
    if (hwnd_ != nullptr) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void Win32Window::set_title(std::string_view title) {
    title_ = widen(title);
    if (hwnd_ != nullptr) {
        SetWindowTextW(hwnd_, title_.c_str());
    }
}

void Win32Window::set_timer(int milliseconds, bool enabled) {
    if (hwnd_ == nullptr) {
        return;
    }
    if (!enabled) {
        KillTimer(hwnd_, kTimerId);
        return;
    }
    SetTimer(hwnd_, kTimerId, static_cast<UINT>(milliseconds), nullptr);
}

void Win32Window::open_file(FileCallback callback, std::string_view title,
                            std::string_view filter) {
    (void)filter;
    static const wchar_t kFilter[] =
        L"KR8 programs\0*.asm;*.s;*.k8;*.bin\0All files\0*.*\0";
    const std::wstring wide_title = widen(title);
    std::vector<wchar_t> path(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = kFilter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = wide_title.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&dialog) == FALSE) {
        callback(std::nullopt);
        return;
    }
    callback(std::filesystem::path(path.data()));
}

void Win32Window::save_file(FileCallback callback, std::string_view title,
                            std::string_view filter,
                            const std::filesystem::path& default_path) {
    (void)filter;
    static const wchar_t kFilter[] =
        L"KR8 source\0*.asm\0All files\0*.*\0";
    std::vector<wchar_t> path(32768, L'\0');
    const std::wstring initial = default_path.wstring();
    if (!initial.empty() && initial.size() + 1 < path.size()) {
        std::copy(initial.begin(), initial.end(), path.begin());
    }
    const std::wstring wide_title = widen(title);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = kFilter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = wide_title.c_str();
    dialog.lpstrDefExt = L"asm";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&dialog) == FALSE) {
        callback(std::nullopt);
        return;
    }
    callback(std::filesystem::path(path.data()));
}

void Win32Window::show_error(std::string_view title,
                             std::string_view message) {
    MessageBoxW(hwnd_, widen(message).c_str(), widen(title).c_str(),
                MB_OK | MB_ICONERROR);
}

std::string Win32Window::clipboard_text() const {
    if (hwnd_ == nullptr || OpenClipboard(hwnd_) == FALSE) {
        return {};
    }
    const HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle == nullptr) {
        CloseClipboard();
        return {};
    }
    const auto* data = static_cast<const wchar_t*>(GlobalLock(handle));
    std::string result;
    if (data != nullptr) {
        result = narrow(data);
        GlobalUnlock(handle);
    }
    CloseClipboard();
    return result;
}

void Win32Window::set_clipboard_text(std::string_view text) {
    if (hwnd_ == nullptr || OpenClipboard(hwnd_) == FALSE) {
        return;
    }
    EmptyClipboard();
    const std::wstring wide = widen(text);
    const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr) {
        void* destination = GlobalLock(memory);
        if (destination != nullptr) {
            std::memcpy(destination, wide.c_str(), bytes);
            GlobalUnlock(memory);
            if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
                GlobalFree(memory);
            }
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
}

void Win32Window::set_cursor(CursorKind cursor) {
    cursor_ = cursor;
    update_cursor();
}

void Win32Window::request_close() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

void Win32Window::update_cursor() {
    LPCWSTR name = IDC_ARROW;
    switch (cursor_) {
        case CursorKind::Arrow: name = IDC_ARROW; break;
        case CursorKind::Hand: name = IDC_HAND; break;
        case CursorKind::Text: name = IDC_IBEAM; break;
        case CursorKind::Wait: name = IDC_WAIT; break;
    }
    SetCursor(LoadCursorW(nullptr, name));
}

LRESULT CALLBACK Win32Window::window_proc(HWND hwnd, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
    Win32Window* window = nullptr;
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<Win32Window*>(create->lpCreateParams);
        window->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Win32Window*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (window != nullptr) {
        return window->handle_message(message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT Win32Window::handle_message(UINT message, WPARAM wparam,
                                    LPARAM lparam) {
    switch (message) {
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize.x = 1024;
            info->ptMinTrackSize.y = 700;
            return 0;
        }
        case WM_SIZE:
            if (application_ != nullptr && wparam != SIZE_MINIMIZED) {
                application_->resize(Size{LOWORD(lparam), HIWORD(lparam)});
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint();
            return 0;
        case WM_TIMER:
            if (application_ != nullptr && wparam == kTimerId) {
                application_->event(Event{Timer{}});
            }
            return 0;
        case WM_MOUSEMOVE: {
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd_;
                TrackMouseEvent(&track);
                tracking_mouse_ = true;
            }
            dispatch_mouse(MouseMove{
                Point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}});
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            dispatch_mouse(MouseLeave{});
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            dispatch_mouse(MouseDown{
                Point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)},
                map_button(message),
                (GetKeyState(VK_SHIFT) & 0x8000) != 0,
                (GetKeyState(VK_CONTROL) & 0x8000) != 0});
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            dispatch_mouse(MouseUp{
                Point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)},
                map_button(message)});
            return 0;
        case WM_MOUSEWHEEL: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd_, &point);
            dispatch_mouse(MouseWheel{Point{point.x, point.y},
                                      GET_WHEEL_DELTA_WPARAM(wparam)});
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            key_down(wparam);
            return 0;
        case WM_CHAR:
            text_input(wparam);
            return 0;
        case WM_SETFOCUS:
            if (application_ != nullptr) {
                application_->event(Event{Focus{true}});
            }
            return 0;
        case WM_KILLFOCUS:
            if (application_ != nullptr) {
                application_->event(Event{Focus{false}});
            }
            return 0;
        case WM_SETCURSOR:
            update_cursor();
            return TRUE;
        case WM_CLOSE:
            if (application_ != nullptr) {
                application_->event(Event{CloseRequest{}});
            } else {
                DestroyWindow(hwnd_);
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimerId);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wparam, lparam);
    }
}

void Win32Window::paint() {
    PAINTSTRUCT paint_struct{};
    HDC dc = BeginPaint(hwnd_, &paint_struct);
    RECT client{};
    GetClientRect(hwnd_, &client);
    if (client.right <= 0 || client.bottom <= 0) {
        EndPaint(hwnd_, &paint_struct);
        return;
    }

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
    const HGDIOBJ old_bitmap = SelectObject(memory, bitmap);

    if (application_ != nullptr) {
        Win32Canvas canvas(memory, client);
        application_->paint(canvas);
    }

    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(hwnd_, &paint_struct);
}

void Win32Window::dispatch_mouse(const Event& event) {
    if (application_ != nullptr) {
        application_->event(event);
    }
}

void Win32Window::key_down(WPARAM key) {
    const Key mapped = map_key(key);
    if (mapped == Key::Unknown || application_ == nullptr) {
        return;
    }
    application_->event(Event{KeyDown{
        mapped, (GetKeyState(VK_CONTROL) & 0x8000) != 0,
        (GetKeyState(VK_SHIFT) & 0x8000) != 0,
        (GetKeyState(VK_MENU) & 0x8000) != 0}});
}

void Win32Window::text_input(WPARAM character) {
    if (application_ == nullptr) {
        return;
    }
    const wchar_t wide[] = {static_cast<wchar_t>(character), L'\0'};
    application_->event(Event{TextInput{narrow(wide)}});
}

}  // namespace kr8::ui::win32
