#include "ui/run.hpp"

#include <filesystem>
#include <string>

#include "ui/app.hpp"
#include "kr8/session.hpp"
#include "sdl_window.hpp"

namespace kr8::ui {

int run_ui(int argc, char** argv) {
    int index = 1;
    if (index < argc &&
        (std::string(argv[index]) == "--ui" ||
         std::string(argv[index]) == "ui")) {
        ++index;
    }
    const bool has_initial = index < argc;

    Session session;
    sdl::SdlWindow window("KR8", 1280, 820);
    App app(window, session);
    if (has_initial) {
        app.open(std::filesystem::path(argv[index]));
    }
    return window.run(app);
}

bool launch_ui_child(const std::filesystem::path& initial,
                     std::string& error) {
    (void)initial;
    error = "the UI runs in-process on Linux";
    return false;
}

}  // namespace kr8::ui
