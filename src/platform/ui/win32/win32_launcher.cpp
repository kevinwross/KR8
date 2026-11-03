#include "win32_launcher.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <system_error>
#include <vector>

namespace kr8::platform {
namespace {

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(),
                        static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::wstring quote(const std::wstring& text) {
    return L"\"" + text + L"\"";
}

}  // namespace

bool launch_ui_child(const std::filesystem::path& initial,
                     std::string& error) {
    std::vector<wchar_t> executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) {
        error = "cannot determine the kr8 executable path";
        return false;
    }
    executable.resize(length);

    const std::wstring executable_path(executable.data(), length);
    std::wstring command = quote(executable_path) + L" --ui";
    if (!initial.empty()) {
        command += L" ";
        command += quote(initial.wstring());
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(executable_path.c_str(), command.data(), nullptr,
                       nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                       &startup, &process) == FALSE) {
        error = std::system_category()
                    .message(static_cast<int>(GetLastError()));
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

}  // namespace kr8::platform
