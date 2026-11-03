#pragma once

#include <filesystem>
#include <string>

namespace kr8::platform {

/// Spawn this executable as a detached `--ui` child with no console.
/// Returns false and fills `error` when CreateProcess fails.
bool launch_ui_child(const std::filesystem::path& initial,
                     std::string& error);

}  // namespace kr8::platform
