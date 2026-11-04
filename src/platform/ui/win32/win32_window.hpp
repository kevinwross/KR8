#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

#include "kr8/ui/window.hpp"

namespace kr8::ui::win32 {

class Win32Window final : public Window {
public:
    Win32Window(std::wstring title, int width = 1280, int height = 820);
    ~Win32Window() override;

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    int run(Application& application) override;
    void invalidate() override;
    void set_title(std::string_view title) override;
    void set_timer(int milliseconds, bool enabled) override;
    void open_file(FileCallback callback, std::string_view title,
                   std::string_view filter) override;
    void save_file(FileCallback callback, std::string_view title,
                   std::string_view filter,
                   const std::filesystem::path& default_path) override;
    void show_error(std::string_view title,
                    std::string_view message) override;
    std::string clipboard_text() const override;
    void set_clipboard_text(std::string_view text) override;
    void set_cursor(CursorKind cursor) override;
    void request_close() override;

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    void key_down(WPARAM key);
    void text_input(WPARAM character);
    void dispatch_mouse(const Event& event);
    void update_cursor();

    std::wstring title_;
    int width_ = 1280;
    int height_ = 820;
    HWND hwnd_ = nullptr;
    Application* application_ = nullptr;
    bool tracking_mouse_ = false;
    CursorKind cursor_ = CursorKind::Arrow;
};

}  // namespace kr8::ui::win32
