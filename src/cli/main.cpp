#include <iostream>
#include <string_view>

#include "kr8/version.hpp"

namespace {

constexpr std::string_view kUsage =
    "KR8 — an 8-bit teaching computer\n"
    "\n"
    "usage: kr8 <command> [options]\n"
    "\n"
    "commands:\n"
    "  as <file.asm>     assemble a source file (M1)\n"
    "  dis <image.k8>    disassemble an image (M1)\n"
    "  sym <image.k8>    print the symbol table (M1)\n"
    "  run <image>       run an image (M2)\n"
    "  ui [file]         open the debugger (M4)\n"
    "  version           print the version\n"
    "  help              print this help\n"
    "\n"
    "The toolchain is under construction; see kr8-impl.md.\n";

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2) {
        const std::string_view command = argv[1];
        if (command == "version" || command == "--version") {
            std::cout << "KR8 " << kr8::version() << "\n";
            return 0;
        }
        if (command == "help" || command == "--help" || command == "-h") {
            std::cout << kUsage;
            return 0;
        }
        std::cerr << "kr8: unknown command '" << command << "'\n\n"
                  << kUsage;
        return 2;
    }

    std::cout << kUsage;
    return 0;
}
