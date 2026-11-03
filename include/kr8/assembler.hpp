#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "kr8/image.hpp"

namespace kr8 {

struct AsmDiagnostic {
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;
    bool warning = false;
};

struct AsmOptions {
    std::string source_name = "<input>";
    bool include_source = true;
};

struct AsmResult {
    bool ok = false;
    Image image;
    std::vector<AsmDiagnostic> diagnostics;
    std::string listing;
    std::string source_text;
    std::string source_name;
    std::uint32_t code_bytes = 0;
    std::uint32_t data_bytes = 0;
};

AsmResult assemble(std::string_view source,
                   const AsmOptions& options = {});
AsmResult assemble_file(const std::filesystem::path& path,
                        const AsmOptions& options = {});

}  // namespace kr8
