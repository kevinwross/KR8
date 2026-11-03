#include "sdl_window.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

namespace kr8::ui::sdl {
namespace {

MouseButton map_button(std::uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        default: return MouseButton::None;
    }
}

Key map_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_RETURN: return Key::Enter;
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_TAB: return Key::Tab;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_DELETE: return Key::Delete;
        case SDLK_UP: return Key::Up;
        case SDLK_DOWN: return Key::Down;
        case SDLK_LEFT: return Key::Left;
        case SDLK_RIGHT: return Key::Right;
        case SDLK_HOME: return Key::Home;
        case SDLK_END: return Key::End;
        case SDLK_PAGEUP: return Key::PageUp;
        case SDLK_PAGEDOWN: return Key::PageDown;
        case SDLK_F1: return Key::F1;
        case SDLK_F5: return Key::F5;
        case SDLK_F6: return Key::F6;
        case SDLK_F7: return Key::F7;
        case SDLK_F8: return Key::F8;
        case SDLK_F10: return Key::F10;
        case SDLK_F11: return Key::F11;
        case SDLK_A: return Key::A;
        case SDLK_C: return Key::C;
        case SDLK_O: return Key::O;
        case SDLK_R: return Key::R;
        case SDLK_S: return Key::S;
        case SDLK_V: return Key::V;
        case SDLK_X: return Key::X;
        case SDLK_Y: return Key::Y;
        case SDLK_Z: return Key::Z;
        default: return Key::Unknown;
    }
}

}  // namespace

SdlWindow::SdlWindow(std::string title, int width, int height)
    : title_(std::move(title)), width_(width), height_(height) {}

SdlWindow::~SdlWindow() {
    canvas_.reset();
    for (SDL_Cursor* cursor : cursors_) {
        if (cursor != nullptr) {
            SDL_DestroyCursor(cursor);
        }
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool SdlWindow::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "kr8: SDL init failed: %s\n", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow(title_.c_str(), width_, height_,
                               SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr) {
        std::fprintf(stderr, "kr8: cannot create window: %s\n",
                     SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "kr8: cannot create renderer: %s\n",
                     SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetWindowMinimumSize(window_, 980, 700);
    try {
        canvas_ = std::make_unique<SdlCanvas>(renderer_);
    } catch (const std::exception& error) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KR8", error.what(),
                                 window_);
        return false;
    }
    return true;
}

int SdlWindow::run(Application& application) {
    if (!initialize()) {
        return 1;
    }
    application_ = &application;
    application_->resize(Size{width_, height_});
    running_ = true;
    while (running_) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            handle_event(event);
        }
        const std::uint64_t now = SDL_GetTicks();
        if (timer_enabled_ && now >= next_timer_) {
            application_->event(Event{Timer{}});
            next_timer_ = now + static_cast<std::uint64_t>(timer_ms_);
        }
        if (dirty_) {
            render();
        }
        SDL_Delay(1);
    }
    return 0;
}

void SdlWindow::invalidate() {
    dirty_ = true;
}

void SdlWindow::set_title(std::string_view title) {
    title_ = std::string(title);
    if (window_ != nullptr) {
        SDL_SetWindowTitle(window_, title_.c_str());
    }
}

void SdlWindow::set_timer(int milliseconds, bool enabled) {
    timer_enabled_ = enabled;
    timer_ms_ = std::max(1, milliseconds);
    next_timer_ = SDL_GetTicks() + static_cast<std::uint64_t>(timer_ms_);
}

void SdlWindow::open_file(FileCallback callback, std::string_view title,
                          std::string_view filter) {
    (void)title;
    (void)filter;
    file_callback_ = std::move(callback);
    // SDL3 expects a semicolon-separated list of bare extensions (or a single
    // "*" for all files); wildcard forms such as "*.c" are rejected, which
    // makes SDL report an error and invoke the callback with no file.
    static const SDL_DialogFileFilter filters[] = {
        {"KR8 files", "asm;s;k8;bin"},
        {"All files", "*"}};
    SDL_ShowOpenFileDialog(&SdlWindow::on_file_selected, this, window_,
                           filters, 2, nullptr, false);
}

void SDLCALL SdlWindow::on_file_selected(void* userdata,
                                         const char* const* files,
                                         int filter) {
    (void)filter;
    auto* window = static_cast<SdlWindow*>(userdata);
    if (!window->file_callback_) {
        return;
    }
    auto callback = std::move(window->file_callback_);
    if (files != nullptr && files[0] != nullptr) {
        callback(std::filesystem::path(files[0]));
    } else {
        callback(std::nullopt);
    }
}

void SdlWindow::show_error(std::string_view title,
                           std::string_view message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, std::string(title).c_str(),
                             std::string(message).c_str(), window_);
}

std::string SdlWindow::clipboard_text() const {
    char* text = SDL_GetClipboardText();
    if (text == nullptr) {
        return {};
    }
    std::string result(text);
    SDL_free(text);
    return result;
}

void SdlWindow::set_clipboard_text(std::string_view text) {
    SDL_SetClipboardText(std::string(text).c_str());
}

void SdlWindow::set_cursor(CursorKind cursor) {
    cursor_ = cursor;
    update_cursor();
}

void SdlWindow::update_cursor() {
    for (SDL_Cursor*& value : cursors_) {
        if (value != nullptr) {
            SDL_DestroyCursor(value);
            value = nullptr;
        }
    }
    SDL_SystemCursor system = SDL_SYSTEM_CURSOR_DEFAULT;
    switch (cursor_) {
        case CursorKind::Arrow: system = SDL_SYSTEM_CURSOR_DEFAULT; break;
        case CursorKind::Hand: system = SDL_SYSTEM_CURSOR_POINTER; break;
        case CursorKind::Text: system = SDL_SYSTEM_CURSOR_TEXT; break;
        case CursorKind::Wait: system = SDL_SYSTEM_CURSOR_WAIT; break;
    }
    cursors_[0] = SDL_CreateSystemCursor(system);
    if (cursors_[0] != nullptr) {
        SDL_SetCursor(cursors_[0]);
    }
}

void SdlWindow::request_close() {
    running_ = false;
}

void SdlWindow::render() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    if (application_ != nullptr && canvas_) {
        application_->paint(*canvas_);
    }
    SDL_RenderPresent(renderer_);
    dirty_ = false;
}

void SdlWindow::handle_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (application_ != nullptr) {
                application_->event(Event{CloseRequest{}});
            } else {
                running_ = false;
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            if (application_ != nullptr) {
                application_->resize(
                    Size{event.window.data1, event.window.data2});
                dirty_ = true;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (application_ != nullptr) {
                application_->event(Event{Focus{true}});
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (application_ != nullptr) {
                application_->event(Event{Focus{false}});
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (application_ != nullptr) {
                application_->event(Event{MouseMove{
                    Point{static_cast<int>(event.motion.x),
                          static_cast<int>(event.motion.y)}}});
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (application_ != nullptr) {
                const Point point{static_cast<int>(event.button.x),
                                  static_cast<int>(event.button.y)};
                const MouseButton button = map_button(event.button.button);
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    const SDL_Keymod mod = SDL_GetModState();
                    application_->event(Event{MouseDown{
                        point, button,
                        (mod & SDL_KMOD_SHIFT) != 0,
                        (mod & SDL_KMOD_CTRL) != 0}});
                } else {
                    application_->event(Event{MouseUp{point, button}});
                }
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (application_ != nullptr) {
                application_->event(Event{MouseWheel{
                    Point{static_cast<int>(event.wheel.mouse_x),
                          static_cast<int>(event.wheel.mouse_y)},
                    static_cast<int>(event.wheel.y * 120.0F)}});
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (application_ != nullptr) {
                application_->event(Event{KeyDown{
                    map_key(event.key.key),
                    (event.key.mod & SDL_KMOD_CTRL) != 0,
                    (event.key.mod & SDL_KMOD_SHIFT) != 0,
                    (event.key.mod & SDL_KMOD_ALT) != 0}});
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (application_ != nullptr) {
                application_->event(
                    Event{TextInput{event.text.text}});
            }
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if (application_ != nullptr) {
                application_->event(Event{MouseLeave{}});
            }
            break;
        default:
            break;
    }
}

}  // namespace kr8::ui::sdl
