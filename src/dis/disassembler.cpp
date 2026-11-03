#include "kr8/disassembler.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "kr8/isa.hpp"

namespace kr8 {
namespace {

std::string hex2(std::uint8_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string text = "0x";
    text.push_back(digits[(value >> 4) & 0x0F]);
    text.push_back(digits[value & 0x0F]);
    return text;
}

std::string hex4(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
        << value;
    return out.str();
}

std::string operand_text(const Image& image, const OpcodeInfo& info,
                         std::uint8_t operand) {
    switch (info.operand) {
        case OperandKind::Target: {
            if (const Symbol* symbol =
                    image.find_symbol(operand, SymbolKind::Code)) {
                return symbol->name;
            }
            return hex2(operand);
        }
        case OperandKind::Address: {
            if (const Symbol* symbol =
                    image.find_symbol(operand, SymbolKind::Data)) {
                return symbol->name;
            }
            if (const Symbol* symbol =
                    image.find_symbol(operand, SymbolKind::Code)) {
                return symbol->name;
            }
            return hex2(operand);
        }
        case OperandKind::Immediate:
            if (const Symbol* symbol =
                    image.find_symbol(operand, SymbolKind::Const)) {
                return symbol->name;
            }
            if (const Symbol* symbol =
                    image.find_symbol(operand, SymbolKind::Data)) {
                return symbol->name;
            }
            return hex2(operand);
        default:
            return hex2(operand);
    }
}

bool is_printable_string(const Image& image, const DataRange& range) {
    for (std::uint16_t i = 0; i < range.size; ++i) {
        const std::uint8_t value =
            image.ram[static_cast<std::size_t>(range.address) + i];
        if (value == 0) {
            return i + 1 == range.size;
        }
        if (value == '\n' || value == '\r' || value == '\t') {
            continue;
        }
        if (value < 0x20 || value > 0x7E) {
            return false;
        }
    }
    return false;
}

std::string format_data(const Image& image, const DataRange& range) {
    std::ostringstream out;
    if (range.symbol != nullptr && range.symbol->kind == SymbolKind::Data) {
        out << range.symbol->name << ": ";
    }
    if (is_printable_string(image, range)) {
        out << ".asciz \"";
        for (std::uint16_t i = 0; i < range.size; ++i) {
            const std::uint8_t value =
                image.ram[static_cast<std::size_t>(range.address) + i];
            if (value == 0) {
                break;
            }
            switch (value) {
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                default: out << static_cast<char>(value); break;
            }
        }
        out << '"';
        return out.str();
    }
    out << ".byte ";
    for (std::uint16_t i = 0; i < range.size; ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << hex2(image.ram[static_cast<std::size_t>(range.address) + i]);
    }
    return out.str();
}

}  // namespace

DecodedImage decode_image(const Image& image) {    DecodedImage decoded;
    int address = 0;
    while (address < 256) {
        const auto code_address = static_cast<std::uint8_t>(address);
        if (!image.is_code(code_address)) {
            if (decoded.data.empty() ||
                decoded.data.back().address +
                        static_cast<std::size_t>(decoded.data.back().size) !=
                    static_cast<std::size_t>(address)) {
                DataRange range;
                range.address = code_address;
                range.size = 0;
                range.symbol = image.find_symbol(code_address);
                decoded.data.push_back(range);
            }
            DataRange& range = decoded.data.back();
            ++range.size;
            ++address;
            continue;
        }

        const std::uint8_t opcode = image.ram[static_cast<std::size_t>(address)];
        const OpcodeInfo* info = info_for_opcode(opcode);
        DecodedInstruction instruction;
        instruction.address = code_address;
        instruction.bytes[0] = opcode;
        if (info == nullptr) {
            instruction.size = 1;
            instruction.text = "???";
            instruction.valid = false;
            decoded.instructions.push_back(std::move(instruction));
            ++address;
            continue;
        }

        instruction.size = info->size;
        instruction.valid = true;
        if (info->size == 2) {
            if (address + 1 >= 256) {
                instruction.size = 1;
                instruction.text = "???";
                instruction.valid = false;
                decoded.instructions.push_back(std::move(instruction));
                ++address;
                continue;
            }
            instruction.bytes[1] =
                image.ram[static_cast<std::size_t>(address + 1)];
        }
        const std::uint8_t operand = info->size == 2 ? instruction.bytes[1] : 0;
        instruction.text =
            format_instruction(*info, operand_text(image, *info, operand));
        instruction.line = image.line_at(code_address);
        decoded.instructions.push_back(std::move(instruction));
        address += info->size;
    }

    for (auto& range : decoded.data) {
        if (range.symbol == nullptr) {
            range.symbol = image.find_symbol(range.address);
        }
        if (range.symbol != nullptr && range.symbol->kind == SymbolKind::Data &&
            range.symbol->size > 0 &&
            range.symbol->size <= range.size) {
            range.size = range.symbol->size;
        }
    }
    return decoded;
}

DecodedImage decode_raw(std::span<const std::uint8_t> bytes,
                        std::uint8_t base, std::uint8_t end) {
    Image image;
    std::copy_n(bytes.begin(), std::min<std::size_t>(bytes.size(), 256),
                image.ram.begin());
    std::uint16_t address = base;
    while (address <= end) {
        const OpcodeInfo* info =
            info_for_opcode(image.ram[static_cast<std::size_t>(address)]);
        image.mark_code(static_cast<std::uint8_t>(address));
        if (info != nullptr &&
            address + static_cast<std::uint16_t>(info->size) - 1 <= end) {
            address = static_cast<std::uint16_t>(address + info->size);
        } else {
            ++address;
        }
    }
    return decode_image(image);
}

std::string disassemble_text(const Image& image,
                             const DisasmOptions& options) {
    return format_decoded(image, decode_image(image), options);
}

std::string format_decoded(const Image& image, const DecodedImage& decoded,
                           const DisasmOptions& options) {
    std::ostringstream out;
    out << "ADDR  BYTES   INSTRUCTION\n";

    std::size_t data_index = 0;
    for (const auto& instruction : decoded.instructions) {
        while (data_index < decoded.data.size() &&
               decoded.data[data_index].address < instruction.address) {
            const DataRange& range = decoded.data[data_index];
            if (options.data && range.size > 0) {
                out << hex4(range.address) << "          "
                    << format_data(image, range) << '\n';
            }
            ++data_index;
        }
        out << hex4(instruction.address) << "  ";
        std::ostringstream bytes;
        for (int i = 0; i < instruction.size; ++i) {
            if (i != 0) {
                bytes << ' ';
            }
            bytes << std::hex << std::uppercase << std::setfill('0')
                  << std::setw(2)
                  << static_cast<int>(instruction.bytes[static_cast<std::size_t>(i)]);
        }
        out << std::left << std::setfill(' ') << std::setw(7) << bytes.str()
            << "  " << instruction.text;
        if (options.source && instruction.line > 0) {
            out << "   ; line " << instruction.line;
        }
        out << '\n';
    }
    while (data_index < decoded.data.size()) {
        const DataRange& range = decoded.data[data_index];
        if (options.data && range.size > 0) {
            out << hex4(range.address) << "          "
                << format_data(image, range) << '\n';
        }
        ++data_index;
    }

    if (options.symbols && !image.symbols.empty()) {
        out << '\n' << symbol_table_text(image);
    }
    return out.str();
}

std::string symbol_table_text(const Image& image) {
    std::ostringstream out;
    out << "Symbols:\n";
    for (const auto& symbol : image.symbols) {
        out << "  " << hex4(static_cast<std::uint32_t>(symbol.value & 0xFFFF))
            << ' ';
        if (symbol.kind == SymbolKind::Code) {
            out << "code ";
        } else if (symbol.kind == SymbolKind::Data) {
            out << "data ";
        } else {
            out << "const";
        }
        out << ' ' << symbol.name;
        if (symbol.kind == SymbolKind::Data) {
            out << " (" << std::dec << symbol.size << " bytes)";
        }
        out << '\n';
    }
    return out.str();
}

}  // namespace kr8
