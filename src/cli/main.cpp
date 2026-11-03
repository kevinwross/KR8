#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "kr8/assembler.hpp"
#include "kr8/disassembler.hpp"
#include "kr8/image.hpp"
#include "kr8/runner.hpp"
#include "kr8/version.hpp"

#ifdef KR8_HAS_UI
#include "ui/run.hpp"
#endif

namespace {

constexpr std::string_view kUsage =
    "KR8 — an 8-bit teaching computer\n"
    "\n"
    "usage: kr8 <command> [options]\n"
    "\n"
    "commands:\n"
    "  as <file.asm>     assemble a source file\n"
    "  dis <image.k8>    disassemble an image\n"
    "  sym <image.k8>    print the symbol table\n"
    "  run <image>       run an image\n"
    "  ui [file]         open the debugger\n"
    "  version           print the version\n"
    "  help              print this help\n"
    "\n"
    "as options:\n"
    "  -o <file.k8>      output image (default: <source>.k8)\n"
    "  --raw <file.bin>  also write the raw 256-byte image\n"
    "  -l, --listing     print the assembler listing\n"
    "  --no-source       do not embed the source text\n"
    "  -q, --quiet       suppress the summary\n"
    "\n"
    "dis options:\n"
    "  --raw             treat the input as a raw .bin\n"
    "  --base <addr>     first raw address (default 0)\n"
    "  --end <addr>      last raw address (default 255)\n"
    "  --symbols         append the symbol table\n"
    "  --source          append source line numbers\n"
    "  --no-data         hide data ranges\n"
    "\n"
    "run options:\n"
    "  --raw             treat the input as a raw .bin\n"
    "  --input <file>    feed input bytes from a file (default: stdin)\n"
    "  --max-instructions <n>  stop after n instructions\n"
    "  --entry <addr>    override the image entry address\n"
    "  --trace           print each instruction to stderr\n"
    "  --stats           print instruction/cycle/time statistics\n";

void print_diagnostics(const kr8::AsmResult& result) {
    for (const auto& diagnostic : result.diagnostics) {
        std::cerr << diagnostic.file;
        if (diagnostic.line > 0) {
            std::cerr << ':' << diagnostic.line;
            if (diagnostic.column > 0) {
                std::cerr << ':' << diagnostic.column;
            }
        }
        std::cerr << ": "
                  << (diagnostic.warning ? "warning: " : "error: ")
                  << diagnostic.message << '\n';
    }
}

int command_as(const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        std::cerr << "kr8 as: missing source file\n\n" << kUsage;
        return 2;
    }

    std::filesystem::path source;
    std::filesystem::path output;
    std::filesystem::path raw;
    bool listing = false;
    bool include_source = true;
    bool quiet = false;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& argument = arguments[i];
        if (argument == "-o") {
            if (i + 1 >= arguments.size()) {
                std::cerr << "kr8 as: -o needs a path\n";
                return 2;
            }
            output = arguments[++i];
        } else if (argument == "--raw") {
            if (i + 1 >= arguments.size()) {
                std::cerr << "kr8 as: --raw needs a path\n";
                return 2;
            }
            raw = arguments[++i];
        } else if (argument == "-l" || argument == "--listing") {
            listing = true;
        } else if (argument == "--no-source") {
            include_source = false;
        } else if (argument == "-q" || argument == "--quiet") {
            quiet = true;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "kr8 as: unknown option '" << argument << "'\n";
            return 2;
        } else if (source.empty()) {
            source = argument;
        } else {
            std::cerr << "kr8 as: more than one source file\n";
            return 2;
        }
    }

    if (source.empty()) {
        std::cerr << "kr8 as: missing source file\n";
        return 2;
    }

    kr8::AsmOptions options;
    options.source_name = source.string();
    options.include_source = include_source;
    const kr8::AsmResult result = kr8::assemble_file(source, options);
    print_diagnostics(result);
    if (!result.ok) {
        return 1;
    }

    if (output.empty()) {
        output = source;
        output.replace_extension(".k8");
    }

    std::string error;
    if (!kr8::save_image_file(output, result.image, error)) {
        std::cerr << "kr8 as: " << error << '\n';
        return 1;
    }
    if (!raw.empty() && !kr8::save_bin_file(raw, result.image, error)) {
        std::cerr << "kr8 as: " << error << '\n';
        return 1;
    }

    if (listing) {
        std::cout << result.listing;
    }
    if (!quiet) {
        const int headroom = 256 - static_cast<int>(result.image.image_high);
        std::cout << output.string() << ": " << result.code_bytes
                  << " code bytes, " << result.data_bytes << " data bytes, "
                  << "image_high=" << result.image.image_high
                  << ", stack headroom=" << headroom << '\n';
    }
    return 0;
}

bool read_file_bytes(const std::filesystem::path& path,
                     std::vector<std::uint8_t>& bytes, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "cannot open file";
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
    return true;
}

bool parse_address(const std::string& text, std::uint8_t& value) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(text, &used, 0);
        if (used != text.size() || parsed < 0 || parsed > 255) {
            return false;
        }
        value = static_cast<std::uint8_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int command_dis(const std::vector<std::string>& arguments) {
    std::filesystem::path input;
    bool raw = false;
    bool symbols = false;
    bool source = false;
    bool show_data = false;
    bool hide_data = false;
    std::uint8_t base = 0;
    std::uint8_t end = 0xFF;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& argument = arguments[i];
        if (argument == "--raw") {
            raw = true;
        } else if (argument == "--symbols") {
            symbols = true;
        } else if (argument == "--source") {
            source = true;
        } else if (argument == "--data") {
            show_data = true;
        } else if (argument == "--no-data") {
            hide_data = true;
        } else if (argument == "--base" || argument == "--end") {
            if (i + 1 >= arguments.size()) {
                std::cerr << "kr8 dis: " << argument << " needs an address\n";
                return 2;
            }
            std::uint8_t value = 0;
            if (!parse_address(arguments[++i], value)) {
                std::cerr << "kr8 dis: invalid address\n";
                return 2;
            }
            if (argument == "--base") {
                base = value;
            } else {
                end = value;
            }
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "kr8 dis: unknown option '" << argument << "'\n";
            return 2;
        } else if (input.empty()) {
            input = argument;
        } else {
            std::cerr << "kr8 dis: more than one input file\n";
            return 2;
        }
    }

    if (input.empty()) {
        std::cerr << "kr8 dis: missing input file\n";
        return 2;
    }

    kr8::DisasmOptions options;
    options.symbols = symbols;
    options.source = source;
    options.data = raw ? show_data : !hide_data;

    if (raw) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!read_file_bytes(input, bytes, error)) {
            std::cerr << "kr8 dis: " << error << '\n';
            return 1;
        }
        if (bytes.size() > 256) {
            std::cerr << "kr8 dis: raw image is larger than 256 bytes\n";
            return 1;
        }
        const kr8::ImageLoadResult loaded = kr8::read_bin(bytes);
        if (!loaded.ok) {
            std::cerr << "kr8 dis: " << loaded.error << '\n';
            return 1;
        }
        const kr8::DecodedImage decoded = kr8::decode_raw(bytes, base, end);
        std::cout << kr8::format_decoded(loaded.image, decoded, options);
        return 0;
    }

    const kr8::ImageLoadResult loaded = kr8::load_image_file(input);
    if (!loaded.ok) {
        std::cerr << "kr8 dis: " << loaded.error << '\n';
        return 1;
    }
    std::cout << kr8::disassemble_text(loaded.image, options);
    return 0;
}

int command_sym(const std::vector<std::string>& arguments) {
    if (arguments.size() != 1) {
        std::cerr << "kr8 sym: expected one image file\n";
        return 2;
    }
    const kr8::ImageLoadResult loaded = kr8::load_image_file(arguments[0]);
    if (!loaded.ok) {
        std::cerr << "kr8 sym: " << loaded.error << '\n';
        return 1;
    }
    if (loaded.image.symbols.empty()) {
        std::cout << "No symbols.\n";
        return 0;
    }
    std::cout << kr8::symbol_table_text(loaded.image);
    return 0;
}

int command_run(const std::vector<std::string>& arguments) {
    std::filesystem::path input;
    std::filesystem::path input_file;
    bool raw = false;
    bool trace = false;
    bool stats = false;
    std::uint64_t max_instructions = 10'000'000;
    std::optional<std::uint8_t> entry;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& argument = arguments[i];
        if (argument == "--raw") {
            raw = true;
        } else if (argument == "--trace") {
            trace = true;
        } else if (argument == "--stats") {
            stats = true;
        } else if (argument == "--input") {
            if (i + 1 >= arguments.size()) {
                std::cerr << "kr8 run: --input needs a path\n";
                return 2;
            }
            input_file = arguments[++i];
        } else if (argument == "--max-instructions") {
            if (i + 1 >= arguments.size()) {
                std::cerr << "kr8 run: --max-instructions needs a count\n";
                return 2;
            }
            try {
                std::size_t used = 0;
                const std::string& text = arguments[++i];
                const unsigned long long parsed = std::stoull(text, &used, 0);
                if (used != text.size()) {
                    throw std::exception();
                }
                max_instructions = parsed;
            } catch (const std::exception&) {
                std::cerr << "kr8 run: invalid instruction budget\n";
                return 2;
            }
        } else if (argument == "--entry") {
            if (i + 1 >= arguments.size()) {
                std::cerr << "kr8 run: --entry needs an address\n";
                return 2;
            }
            std::uint8_t value = 0;
            if (!parse_address(arguments[++i], value)) {
                std::cerr << "kr8 run: invalid entry address\n";
                return 2;
            }
            entry = value;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "kr8 run: unknown option '" << argument << "'\n";
            return 2;
        } else if (input.empty()) {
            input = argument;
        } else {
            std::cerr << "kr8 run: more than one input file\n";
            return 2;
        }
    }

    if (input.empty()) {
        std::cerr << "kr8 run: missing input file\n";
        return 2;
    }

    kr8::ImageLoadResult loaded;
    if (raw) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!read_file_bytes(input, bytes, error)) {
            std::cerr << "kr8 run: " << error << '\n';
            return 1;
        }
        if (bytes.size() > 256) {
            std::cerr << "kr8 run: raw image is larger than 256 bytes\n";
            return 1;
        }
        loaded = kr8::read_bin(bytes);
    } else {
        loaded = kr8::load_image_file(input);
    }
    if (!loaded.ok) {
        std::cerr << "kr8 run: " << loaded.error << '\n';
        return 1;
    }

    kr8::RunOptions options;
    options.entry = entry;
    options.max_instructions = max_instructions;
    options.trace = trace;
    if (!input_file.empty()) {
        std::string error;
        if (!read_file_bytes(input_file, options.input, error)) {
            std::cerr << "kr8 run: " << error << '\n';
            return 1;
        }
    } else {
        options.read_stdin = true;
    }

    const auto started = std::chrono::steady_clock::now();
    const kr8::RunResult result = kr8::run_image(loaded.image, options);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);

    std::cout.write(result.output.data(),
                    static_cast<std::streamsize>(result.output.size()));
    std::cout.flush();

    if (trace && !result.trace.empty()) {
        std::cerr << result.trace;
    }
    if (stats) {
        std::cerr << "instructions=" << result.instructions
                  << " cycles=" << result.cycles
                  << " output_bytes=" << result.output.size()
                  << " elapsed_ms=" << elapsed.count() << '\n';
    }

    if (result.status == kr8::RunStatus::Halted) {
        return 0;
    }
    if (result.status == kr8::RunStatus::Break) {
        std::cerr << "kr8 run: BRK\n";
        return 0;
    }
    if (result.status == kr8::RunStatus::Fault) {
        std::cerr << "kr8 run: " << result.message << '\n';
        return 2;
    }
    std::cerr << "kr8 run: " << result.message << '\n';
    return 3;
}

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
        std::vector<std::string> arguments;
        for (int i = 2; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }
        if (command == "as") {
            return command_as(arguments);
        }
        if (command == "dis") {
            return command_dis(arguments);
        }
        if (command == "sym") {
            return command_sym(arguments);
        }
        if (command == "run") {
            return command_run(arguments);
        }
#ifdef KR8_HAS_UI
        if (command == "ui" || command == "--ui") {
            return kr8::ui::run_ui(argc, argv);
        }
#endif
        std::cerr << "kr8: unknown command '" << command << "'\n\n"
                  << kUsage;
        return 2;
    }

#ifdef KR8_HAS_UI
    {
        std::string error;
        if (kr8::ui::launch_ui_child({}, error)) {
            return 0;
        }
        std::cerr << "kr8: " << error << '\n';
    }
#endif
    std::cout << kUsage;
    return 0;
}
