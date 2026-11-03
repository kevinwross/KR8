#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "kr8/ui/canvas.hpp"
#include "kr8/ui/geometry.hpp"
#include "kr8/ui/input.hpp"

namespace kr8::ui {

enum class CursorKind {
    Arrow,
    Hand,
    Text,
    Wait,
};

/// The portable UI implements this. The backend owns the native window and
/// calls paint/event/resize on the UI thread.
class Application {
public:
    virtual ~Application() = default;
    virtual void paint(Canvas& canvas) = 0;
    virtual void event(const Event& event) = 0;
    virtual void resize(Size size) = 0;
};

/// Called exactly once with the selected file, or with no value when the
/// dialog is cancelled. Backends may complete this asynchronously.
using FileCallback =
    std::function<void(std::optional<std::filesystem::path>)>;

/// Window services used by the portable UI. The backend owns the native
/// window and the event loop; the UI owns layout, state and painting.
class Window {
public:
    virtual ~Window() = default;

    virtual int run(Application& application) = 0;
    virtual void invalidate() = 0;
    virtual void set_title(std::string_view title) = 0;
    virtual void set_timer(int milliseconds, bool enabled) = 0;
    virtual void open_file(FileCallback callback, std::string_view title,
                           std::string_view filter) = 0;
    virtual void show_error(std::string_view title,
                            std::string_view message) = 0;
    virtual std::string clipboard_text() const = 0;
    virtual void set_clipboard_text(std::string_view text) = 0;
    virtual void set_cursor(CursorKind cursor) = 0;
    virtual void request_close() = 0;
};

}  // namespace kr8::ui
