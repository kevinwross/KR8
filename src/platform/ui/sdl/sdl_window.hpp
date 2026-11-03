#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>

#include "kr8/ui/window.hpp"
#include "sdl_canvas.hpp"

namespace kr8::ui::sdl {

class SdlWindow final : public Window {
public:
    SdlWindow(std::string title, int width = 1280, int height = 820);
    ~SdlWindow() override;

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow& operator=(const SdlWindow&) = delete;

    int run(Application& application) override;
    void invalidate() override;
    void set_title(std::string_view title) override;
    void set_timer(int milliseconds, bool enabled) override;
    void open_file(FileCallback callback, std::string_view title,
                   std::string_view filter) override;
    void show_error(std::string_view title,
                    std::string_view message) override;
    std::string clipboard_text() const override;
    void set_clipboard_text(std::string_view text) override;
    void set_cursor(CursorKind cursor) override;
    void request_close() override;

private:
    bool initialize();
    void handle_event(const SDL_Event& event);
    void render();
    void update_cursor();
    static void SDLCALL on_file_selected(void* userdata,
                                         const char* const* files,
                                         int filter);

    std::string title_;
    int width_ = 1280;
    int height_ = 820;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<SdlCanvas> canvas_;
    Application* application_ = nullptr;
    bool running_ = false;
    bool dirty_ = true;
    bool timer_enabled_ = false;
    int timer_ms_ = 8;
    std::uint64_t next_timer_ = 0;
    FileCallback file_callback_;
    CursorKind cursor_ = CursorKind::Arrow;
    SDL_Cursor* cursors_[4]{};
};

}  // namespace kr8::ui::sdl
