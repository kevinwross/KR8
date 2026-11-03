#pragma once

#include <filesystem>
#include <string>

namespace kr8::ui {

/// Run the windowed UI. `argv[1]` may be `--ui` and `argv[2]` an optional file.
int run_ui(int argc, char** argv);

/// Spawn this executable with `--ui` and no console.
bool launch_ui_child(const std::filesystem::path& initial,
                     std::string& error);

}  // namespace kr8::ui
