#include "kr8/assembler.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "kr8/isa.hpp"

namespace kr8 {
namespace {

using SymbolMap = std::unordered_map<std::string, Symbol>;

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

char upper_ascii(char value) {
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

std::string upper(std::string_view text) {
    std::string result(text);
    for (char& value : result) {
        value = upper_ascii(value);
    }
    return result;
}

bool equal_ci(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (upper_ascii(left[i]) != upper_ascii(right[i])) {
            return false;
        }
    }
    return true;
}

bool is_ident_start(char value) {
    return std::isalpha(static_cast<unsigned char>(value)) != 0 ||
           value == '_' || value == '.';
}

bool is_ident_char(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
           value == '_' || value == '.';
}

bool is_identifier(std::string_view text) {
    if (text.empty() || !is_ident_start(text.front())) {
        return false;
    }
    for (const char value : text) {
        if (!is_ident_char(value)) {
            return false;
        }
    }
    return true;
}

std::string normalize_source(std::string_view source) {
    std::string result;
    result.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const char value = source[i];
        if (value == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') {
                ++i;
            }
            result.push_back('\n');
        } else {
            result.push_back(value);
        }
    }
    return result;
}

std::vector<std::string> split_lines(const std::string& source) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < source.size()) {
        const std::size_t end = source.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(source.substr(start));
            break;
        }
        lines.push_back(source.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

std::string strip_comment(std::string_view line) {
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char value = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (value == '\\' && (in_string || in_char)) {
            escaped = true;
            continue;
        }
        if (value == '"' && !in_char) {
            in_string = !in_string;
            continue;
        }
        if (value == '\'' && !in_string) {
            in_char = !in_char;
            continue;
        }
        if (value == ';' && !in_string && !in_char) {
            return std::string(line.substr(0, i));
        }
    }
    return std::string(line);
}

struct OperandSplit {
    bool ok = false;
    std::vector<std::string> parts;
    std::string error;
};

OperandSplit split_operands(std::string_view text) {
    OperandSplit result;
    if (trim(text).empty()) {
        result.ok = true;
        return result;
    }
    int depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char value = text[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (value == '\\' && (in_string || in_char)) {
            escaped = true;
            continue;
        }
        if (value == '"' && !in_char) {
            in_string = !in_string;
            continue;
        }
        if (value == '\'' && !in_string) {
            in_char = !in_char;
            continue;
        }
        if (in_string || in_char) {
            continue;
        }
        if (value == '[') {
            ++depth;
        } else if (value == ']') {
            --depth;
            if (depth < 0) {
                result.error = "unmatched ']'";
                return result;
            }
        } else if (value == ',' && depth == 0) {
            result.parts.push_back(trim(text.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (in_string || in_char) {
        result.error = "unterminated string or character literal";
        return result;
    }
    if (depth != 0) {
        result.error = "unmatched '['";
        return result;
    }
    result.parts.push_back(trim(text.substr(start)));
    for (const auto& part : result.parts) {
        if (part.empty()) {
            result.error = "empty operand";
            return result;
        }
    }
    result.ok = true;
    return result;
}

bool decode_escape(char value, int& decoded) {
    switch (value) {
        case 'n': decoded = '\n'; return true;
        case 'r': decoded = '\r'; return true;
        case 't': decoded = '\t'; return true;
        case '0': decoded = 0; return true;
        case '\\': decoded = '\\'; return true;
        case '\'': decoded = '\''; return true;
        case '"': decoded = '"'; return true;
        default: return false;
    }
}

bool parse_char_literal(std::string_view text, std::uint8_t& value,
                        std::string& error) {
    if (text.size() < 3 || text.front() != '\'' || text.back() != '\'') {
        error = "invalid character literal";
        return false;
    }
    const std::string inner = std::string(text.substr(1, text.size() - 2));
    if (inner.empty()) {
        error = "empty character literal";
        return false;
    }
    if (inner[0] != '\\') {
        if (inner.size() != 1) {
            error = "character literal must contain one character";
            return false;
        }
        value = static_cast<std::uint8_t>(inner[0]);
        return true;
    }
    if (inner.size() < 2) {
        error = "incomplete escape";
        return false;
    }
    const char escape = inner[1];
    if (escape == 'x') {
        if (inner.size() != 4) {
            error = "\\x escape needs exactly two hex digits";
            return false;
        }
        int decoded = 0;
        for (int i = 0; i < 2; ++i) {
            const char digit = inner[2 + static_cast<std::size_t>(i)];
            decoded <<= 4;
            if (digit >= '0' && digit <= '9') {
                decoded += digit - '0';
            } else if (digit >= 'a' && digit <= 'f') {
                decoded += digit - 'a' + 10;
            } else if (digit >= 'A' && digit <= 'F') {
                decoded += digit - 'A' + 10;
            } else {
                error = "invalid hex digit in \\x escape";
                return false;
            }
        }
        value = static_cast<std::uint8_t>(decoded);
        return true;
    }
    if (inner.size() != 2) {
        error = "invalid escape sequence";
        return false;
    }
    int decoded = 0;
    if (!decode_escape(escape, decoded)) {
        error = "unknown escape sequence";
        return false;
    }
    value = static_cast<std::uint8_t>(decoded);
    return true;
}

bool parse_string_literal(std::string_view text, std::string& value,
                          std::string& error) {
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
        error = "expected a quoted string";
        return false;
    }
    value.clear();
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        char current = text[i];
        if (current != '\\') {
            value.push_back(current);
            continue;
        }
        if (i + 2 >= text.size()) {
            error = "incomplete escape";
            return false;
        }
        const char escape = text[++i];
        if (escape == 'x') {
            if (i + 2 >= text.size()) {
                error = "\\x escape needs two hex digits";
                return false;
            }
            int decoded = 0;
            for (int digit_index = 0; digit_index < 2; ++digit_index) {
                const char digit = text[++i];
                decoded <<= 4;
                if (digit >= '0' && digit <= '9') {
                    decoded += digit - '0';
                } else if (digit >= 'a' && digit <= 'f') {
                    decoded += digit - 'a' + 10;
                } else if (digit >= 'A' && digit <= 'F') {
                    decoded += digit - 'A' + 10;
                } else {
                    error = "invalid hex digit in \\x escape";
                    return false;
                }
            }
            value.push_back(static_cast<char>(decoded));
            continue;
        }
        switch (escape) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case '0': value.push_back('\0'); break;
            case '\\': value.push_back('\\'); break;
            case '\'': value.push_back('\''); break;
            case '"': value.push_back('"'); break;
            default:
                error = "unknown escape sequence";
                return false;
        }
    }
    return true;
}

enum class ExprToken {
    End,
    Number,
    Symbol,
    Character,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Amp,
    Pipe,
    Caret,
    Tilde,
    ShiftLeft,
    ShiftRight,
    LParen,
    RParen,
    Invalid,
};

struct ExprLexeme {
    ExprToken type = ExprToken::End;
    std::int64_t value = 0;
    std::string name;
    std::size_t position = 0;
};

class ExprParser {
public:
    ExprParser(std::string_view text, const SymbolMap& symbols)
        : text_(text), symbols_(symbols) {}

    std::optional<std::int64_t> parse(std::string& error,
                                      std::size_t& error_position) {
        advance();
        const std::optional<std::int64_t> value = parse_binary(0);
        if (failed_) {
            error = error_;
            error_position = error_position_;
            return std::nullopt;
        }
        if (!value.has_value()) {
            error = "empty expression";
            error_position = 0;
            return std::nullopt;
        }
        if (current_.type != ExprToken::End) {
            error = "unexpected token after expression";
            error_position = current_.position;
            return std::nullopt;
        }
        return value;
    }

private:
    void fail(std::string message, std::size_t position) {
        if (!failed_) {
            failed_ = true;
            error_ = std::move(message);
            error_position_ = position;
        }
    }

    static bool is_space(char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    }

    static int precedence(ExprToken type) {
        switch (type) {
            case ExprToken::Pipe: return 1;
            case ExprToken::Caret: return 2;
            case ExprToken::Amp: return 3;
            case ExprToken::ShiftLeft:
            case ExprToken::ShiftRight: return 4;
            case ExprToken::Plus:
            case ExprToken::Minus: return 5;
            case ExprToken::Star:
            case ExprToken::Slash:
            case ExprToken::Percent: return 6;
            default: return -1;
        }
    }

    int hex_digit(char value) const {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F') {
            return value - 'A' + 10;
        }
        return -1;
    }

    bool parse_number(std::int64_t& value, std::size_t start,
                      std::size_t& end) {
        std::size_t i = start;
        int base = 10;
        if (text_[i] == '$') {
            base = 16;
            ++i;
            if (i >= text_.size()) {
                fail("expected hex digits after '$'", start);
                return false;
            }
        } else if (text_[i] == '0' && i + 1 < text_.size() &&
                   (text_[i + 1] == 'x' || text_[i + 1] == 'X')) {
            base = 16;
            i += 2;
        } else if (text_[i] == '0' && i + 1 < text_.size() &&
                   (text_[i + 1] == 'b' || text_[i + 1] == 'B')) {
            base = 2;
            i += 2;
        }
        const std::size_t digit_start = i;
        std::uint64_t result = 0;
        while (i < text_.size()) {
            const int digit = hex_digit(text_[i]);
            if (digit < 0 || digit >= base) {
                break;
            }
            if (result >
                (static_cast<std::uint64_t>(
                     std::numeric_limits<std::int64_t>::max()) -
                 static_cast<std::uint64_t>(digit)) /
                    static_cast<std::uint64_t>(base)) {
                fail("numeric literal is too large", start);
                return false;
            }
            result = result * static_cast<std::uint64_t>(base) +
                     static_cast<std::uint64_t>(digit);
            ++i;
        }
        if (i == digit_start) {
            fail("expected digits", start);
            return false;
        }
        value = static_cast<std::int64_t>(result);
        end = i;
        return true;
    }

    void advance() {
        if (failed_) {
            return;
        }
        std::size_t i = position_;
        while (i < text_.size() && is_space(text_[i])) {
            ++i;
        }
        if (i >= text_.size()) {
            current_ = ExprLexeme{ExprToken::End, 0, "", i};
            position_ = i;
            return;
        }
        const std::size_t start = i;
        const char value = text_[i];
        if (std::isdigit(static_cast<unsigned char>(value)) != 0 ||
            value == '$') {
            std::int64_t number = 0;
            std::size_t end = i;
            if (!parse_number(number, i, end)) {
                current_.type = ExprToken::Invalid;
                position_ = end;
                return;
            }
            current_ = ExprLexeme{ExprToken::Number, number, "", start};
            position_ = end;
            return;
        }
        if (is_ident_start(value)) {
            ++i;
            while (i < text_.size() && is_ident_char(text_[i])) {
                ++i;
            }
            current_ = ExprLexeme{
                ExprToken::Symbol, 0, std::string(text_.substr(start, i - start)),
                start};
            position_ = i;
            return;
        }
        if (value == '\'') {
            std::size_t close = i + 1;
            bool escaped = false;
            for (; close < text_.size(); ++close) {
                const char current = text_[close];
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (current == '\\') {
                    escaped = true;
                    continue;
                }
                if (current == '\'') {
                    break;
                }
            }
            if (close >= text_.size()) {
                fail("unterminated character literal", start);
                return;
            }
            std::uint8_t decoded = 0;
            std::string error;
            if (!parse_char_literal(text_.substr(i, close - i + 1), decoded,
                                    error)) {
                fail(error, start);
                return;
            }
            current_ = ExprLexeme{ExprToken::Character, decoded, "", start};
            position_ = close + 1;
            return;
        }
        switch (value) {
            case '+': current_ = {ExprToken::Plus, 0, "", start}; ++i; break;
            case '-': current_ = {ExprToken::Minus, 0, "", start}; ++i; break;
            case '*': current_ = {ExprToken::Star, 0, "", start}; ++i; break;
            case '/': current_ = {ExprToken::Slash, 0, "", start}; ++i; break;
            case '%': current_ = {ExprToken::Percent, 0, "", start}; ++i; break;
            case '&': current_ = {ExprToken::Amp, 0, "", start}; ++i; break;
            case '|': current_ = {ExprToken::Pipe, 0, "", start}; ++i; break;
            case '^': current_ = {ExprToken::Caret, 0, "", start}; ++i; break;
            case '~': current_ = {ExprToken::Tilde, 0, "", start}; ++i; break;
            case '(': current_ = {ExprToken::LParen, 0, "", start}; ++i; break;
            case ')': current_ = {ExprToken::RParen, 0, "", start}; ++i; break;
            case '<':
                if (i + 1 < text_.size() && text_[i + 1] == '<') {
                    current_ = {ExprToken::ShiftLeft, 0, "", start};
                    i += 2;
                } else {
                    fail("unexpected '<'", start);
                }
                break;
            case '>':
                if (i + 1 < text_.size() && text_[i + 1] == '>') {
                    current_ = {ExprToken::ShiftRight, 0, "", start};
                    i += 2;
                } else {
                    fail("unexpected '>'", start);
                }
                break;
            default:
                fail(std::string("unexpected character '") + value + "'",
                     start);
                break;
        }
        position_ = i;
    }

    std::optional<std::int64_t> parse_primary() {
        if (failed_) {
            return std::nullopt;
        }
        const ExprLexeme token = current_;
        switch (token.type) {
            case ExprToken::Number:
            case ExprToken::Character:
                advance();
                return token.value;
            case ExprToken::Symbol: {
                const auto found = symbols_.find(token.name);
                if (found == symbols_.end()) {
                    fail("undefined symbol '" + token.name + "'",
                         token.position);
                    return std::nullopt;
                }
                advance();
                return static_cast<std::int64_t>(found->second.value);
            }
            case ExprToken::LParen: {
                advance();
                const auto value = parse_binary(0);
                if (!value.has_value()) {
                    return std::nullopt;
                }
                if (current_.type != ExprToken::RParen) {
                    fail("expected ')'", current_.position);
                    return std::nullopt;
                }
                advance();
                return value;
            }
            case ExprToken::Plus:
                advance();
                return parse_primary();
            case ExprToken::Minus: {
                advance();
                const auto value = parse_primary();
                if (!value.has_value()) {
                    return std::nullopt;
                }
                return -*value;
            }
            case ExprToken::Tilde: {
                advance();
                const auto value = parse_primary();
                if (!value.has_value()) {
                    return std::nullopt;
                }
                return ~*value;
            }
            default:
                fail("expected an expression", token.position);
                return std::nullopt;
        }
    }

    std::optional<std::int64_t> parse_binary(int min_precedence) {
        auto left = parse_primary();
        if (!left.has_value()) {
            return std::nullopt;
        }
        while (!failed_) {
            const int precedence_value = precedence(current_.type);
            if (precedence_value < min_precedence) {
                break;
            }
            const ExprToken operation = current_.type;
            const std::size_t operation_position = current_.position;
            advance();
            const auto right = parse_binary(precedence_value + 1);
            if (!right.has_value()) {
                return std::nullopt;
            }
            switch (operation) {
                case ExprToken::Plus: *left += *right; break;
                case ExprToken::Minus: *left -= *right; break;
                case ExprToken::Star: *left *= *right; break;
                case ExprToken::Slash:
                    if (*right == 0) {
                        fail("division by zero", operation_position);
                        return std::nullopt;
                    }
                    *left /= *right;
                    break;
                case ExprToken::Percent:
                    if (*right == 0) {
                        fail("remainder by zero", operation_position);
                        return std::nullopt;
                    }
                    *left %= *right;
                    break;
                case ExprToken::Amp: *left &= *right; break;
                case ExprToken::Pipe: *left |= *right; break;
                case ExprToken::Caret: *left ^= *right; break;
                case ExprToken::ShiftLeft:
                case ExprToken::ShiftRight:
                    if (*right < 0 || *right > 63) {
                        fail("shift count is out of range",
                             operation_position);
                        return std::nullopt;
                    }
                    if (operation == ExprToken::ShiftLeft) {
                        *left = static_cast<std::int64_t>(
                            static_cast<std::uint64_t>(*left) << *right);
                    } else {
                        *left >>= *right;
                    }
                    break;
                default:
                    fail("internal expression error", operation_position);
                    return std::nullopt;
            }
        }
        return left;
    }

    std::string_view text_;
    const SymbolMap& symbols_;
    std::size_t position_ = 0;
    ExprLexeme current_;
    bool failed_ = false;
    std::string error_;
    std::size_t error_position_ = 0;
};

enum class Form {
    None,
    Immediate,
    Address,
    Target,
};

struct ParsedInstruction {
    bool ok = false;
    Op op = Op::Hlt;
    Form form = Form::None;
    std::string expr;
    std::string error;
};

bool is_bracket_operand(std::string_view text, std::string& inner) {
    const std::string trimmed = trim(text);
    if (trimmed.size() < 2 || trimmed.front() != '[' ||
        trimmed.back() != ']') {
        return false;
    }
    inner = trim(std::string_view(trimmed).substr(1, trimmed.size() - 2));
    return true;
}

bool is_immediate_operand(std::string_view text, std::string& inner) {
    const std::string trimmed = trim(text);
    if (trimmed.empty() || trimmed.front() != '#') {
        return false;
    }
    inner = trim(std::string_view(trimmed).substr(1));
    return !inner.empty();
}

int register_code(std::string_view text) {
    const std::string trimmed = trim(text);
    if (equal_ci(trimmed, "A")) {
        return 0;
    }
    if (equal_ci(trimmed, "B")) {
        return 1;
    }
    return -1;
}

ParsedInstruction fail_instruction(std::string message) {
    ParsedInstruction result;
    result.error = std::move(message);
    return result;
}

ParsedInstruction make_instruction(Op op, Form form = Form::None,
                                   std::string expr = {}) {
    ParsedInstruction result;
    result.ok = true;
    result.op = op;
    result.form = form;
    result.expr = std::move(expr);
    return result;
}

bool alu_pair(const std::string& base, Op& register_op, Op& immediate_op) {
    if (base == "ADD") { register_op = Op::AddB; immediate_op = Op::AddImm; }
    else if (base == "ADC") { register_op = Op::AdcB; immediate_op = Op::AdcImm; }
    else if (base == "SUB") { register_op = Op::SubB; immediate_op = Op::SubImm; }
    else if (base == "SBC") { register_op = Op::SbcB; immediate_op = Op::SbcImm; }
    else if (base == "CMP") { register_op = Op::CmpB; immediate_op = Op::CmpImm; }
    else if (base == "AND") { register_op = Op::AndB; immediate_op = Op::AndImm; }
    else if (base == "OR") { register_op = Op::OrB; immediate_op = Op::OrImm; }
    else if (base == "XOR") { register_op = Op::XorB; immediate_op = Op::XorImm; }
    else if (base == "TST") { register_op = Op::TstB; immediate_op = Op::TstImm; }
    else { return false; }
    return true;
}

ParsedInstruction parse_instruction(std::string_view mnemonic,
                                    const std::vector<std::string>& operands) {
    const std::string base = upper(mnemonic);

    const OpcodeInfo* branch = find_mnemonic(base);
    if (branch != nullptr && is_branch(branch->op)) {
        if (operands.size() != 1) {
            return fail_instruction("expected one branch target");
        }
        if (operands[0].front() == '#' || operands[0].front() == '[') {
            return fail_instruction("branch target must be an absolute address");
        }
        return make_instruction(branch->op, Form::Target, operands[0]);
    }

    if (base == "HLT" || base == "NOP" || base == "BRK" || base == "RET" ||
        base == "CLC" || base == "SEC") {
        if (!operands.empty()) {
            return fail_instruction("this instruction takes no operands");
        }
        static const std::unordered_map<std::string, Op> kOps = {
            {"HLT", Op::Hlt}, {"NOP", Op::Nop}, {"BRK", Op::Brk},
            {"RET", Op::Ret}, {"CLC", Op::Clc}, {"SEC", Op::Sec},
        };
        return make_instruction(kOps.at(base));
    }

    if (base == "PUSH" || base == "POP") {
        if (operands.size() != 1) {
            return fail_instruction("expected A or B");
        }
        const int reg = register_code(operands[0]);
        if (reg < 0) {
            return fail_instruction("expected A or B");
        }
        if (base == "PUSH") {
            return make_instruction(reg == 0 ? Op::PushA : Op::PushB);
        }
        return make_instruction(reg == 0 ? Op::PopA : Op::PopB);
    }

    if (base == "MOV") {
        if (operands.size() != 2) {
            return fail_instruction("MOV needs two operands");
        }
        const int first = register_code(operands[0]);
        if (first < 0) {
            return fail_instruction("MOV destination must be A or B");
        }
        const int second = register_code(operands[1]);
        if (second >= 0) {
            if (first == 0 && second == 1) {
                return make_instruction(Op::MovAB);
            }
            if (first == 1 && second == 0) {
                return make_instruction(Op::MovBA);
            }
            return fail_instruction("MOV needs a different source register");
        }
        std::string expression;
        if (is_immediate_operand(operands[1], expression)) {
            return make_instruction(first == 0 ? Op::MovAImm : Op::MovBImm,
                                    Form::Immediate, expression);
        }
        return fail_instruction("MOV source must be a register or #immediate");
    }

    if (base == "LD") {
        if (operands.size() != 2) {
            return fail_instruction("LD needs two operands");
        }
        const int first = register_code(operands[0]);
        if (first < 0) {
            return fail_instruction("LD destination must be A or B");
        }
        std::string inner;
        if (!is_bracket_operand(operands[1], inner)) {
            return fail_instruction("LD source must be [address] or [B]");
        }
        if (equal_ci(inner, "B")) {
            if (first != 0) {
                return fail_instruction("LD B, [B] is not supported");
            }
            return make_instruction(Op::LdAB);
        }
        if (inner.empty()) {
            return fail_instruction("empty address");
        }
        return make_instruction(first == 0 ? Op::LdAAddr : Op::LdBAddr,
                                Form::Address, inner);
    }

    if (base == "ST") {
        if (operands.size() != 2) {
            return fail_instruction("ST needs two operands");
        }
        std::string inner;
        if (!is_bracket_operand(operands[0], inner)) {
            return fail_instruction("ST destination must be [address] or [B]");
        }
        const int second = register_code(operands[1]);
        if (second < 0) {
            return fail_instruction("ST source must be A or B");
        }
        if (equal_ci(inner, "B")) {
            if (second != 0) {
                return fail_instruction("ST [B], B is not supported");
            }
            return make_instruction(Op::StAB);
        }
        if (inner.empty()) {
            return fail_instruction("empty address");
        }
        return make_instruction(second == 0 ? Op::StAAddr : Op::StBAddr,
                                Form::Address, inner);
    }

    Op register_op = Op::Hlt;
    Op immediate_op = Op::Hlt;
    if (alu_pair(base, register_op, immediate_op)) {
        if (operands.size() != 2) {
            return fail_instruction(base + " needs two operands");
        }
        if (register_code(operands[0]) != 0) {
            return fail_instruction(base + " destination must be A");
        }
        if (register_code(operands[1]) == 1) {
            return make_instruction(register_op);
        }
        std::string expression;
        if (is_immediate_operand(operands[1], expression)) {
            return make_instruction(immediate_op, Form::Immediate, expression);
        }
        return fail_instruction(base + " source must be B or #immediate");
    }

    if (base == "NOT" || base == "NEG" || base == "SHL" || base == "SHR" ||
        base == "SAR") {
        if (operands.size() != 1 || register_code(operands[0]) != 0) {
            return fail_instruction(base + " needs A");
        }
        static const std::unordered_map<std::string, Op> kOps = {
            {"NOT", Op::NotA}, {"NEG", Op::NegA}, {"SHL", Op::ShlA},
            {"SHR", Op::ShrA}, {"SAR", Op::SarA},
        };
        return make_instruction(kOps.at(base));
    }

    if (base == "INC" || base == "DEC") {
        if (operands.size() != 1) {
            return fail_instruction(base + " needs A or B");
        }
        const int reg = register_code(operands[0]);
        if (reg < 0) {
            return fail_instruction(base + " needs A or B");
        }
        if (base == "INC") {
            return make_instruction(reg == 0 ? Op::IncA : Op::IncB);
        }
        return make_instruction(reg == 0 ? Op::DecA : Op::DecB);
    }

    if (base == "OUT" || base == "IN") {
        if (operands.size() != 1 || register_code(operands[0]) != 0) {
            return fail_instruction(base + " needs A");
        }
        return make_instruction(base == "OUT" ? Op::OutA : Op::InA);
    }

    return fail_instruction("unknown mnemonic '" + std::string(mnemonic) + "'");
}

enum class ItemKind {
    Instruction,
    Bytes,
    Words,
    Ascii,
    Asciz,
    Space,
};

struct Item {
    ItemKind kind = ItemKind::Instruction;
    int line = 0;
    int address = 0;
    int size = 0;
    Op op = Op::Hlt;
    Form form = Form::None;
    std::string expr;
    std::vector<std::string> exprs;
    std::vector<std::string> strings;
};

struct AssemblyState {
    AsmOptions options;
    std::string source;
    std::vector<std::string> source_lines;
    SymbolMap symbols;
    std::vector<std::string> symbol_order;
    std::vector<Item> items;
    std::vector<std::string> pending_labels;
    int location = 0;
    int high_water = 0;
    int first_instruction = -1;
    std::optional<int> entry_directive;
    bool had_error = false;
    std::vector<AsmDiagnostic> diagnostics;
    AsmResult result;

    void error(int line, int column, std::string message) {
        diagnostics.push_back(
            AsmDiagnostic{options.source_name, line, column,
                          std::move(message), false});
        had_error = true;
    }

    void error(int line, std::string message) {
        error(line, 1, std::move(message));
    }

    void warning(int line, int column, std::string message) {
        diagnostics.push_back(
            AsmDiagnostic{options.source_name, line, column,
                          std::move(message), true});
    }

    void warning(int line, std::string message) {
        warning(line, 1, std::move(message));
    }

    std::optional<std::int64_t> evaluate(std::string_view expression, int line,
                                         int column) {
        ExprParser parser(expression, symbols);
        std::string message;
        std::size_t position = 0;
        const auto value = parser.parse(message, position);
        if (!value.has_value()) {
            error(line, column + static_cast<int>(position),
                  std::move(message));
            return std::nullopt;
        }
        return value;
    }

    bool define_symbol(std::string name, std::int32_t value, SymbolKind kind,
                       std::uint16_t size, int line, int column) {
        const auto found = symbols.find(name);
        if (found != symbols.end()) {
            error(line, column, "duplicate symbol '" + name + "'");
            return false;
        }
        symbols.emplace(name, Symbol{name, value, kind, size});
        symbol_order.push_back(std::move(name));
        return true;
    }

    void flush_labels(SymbolKind kind, std::uint16_t size, int line,
                      int column) {
        if (pending_labels.empty()) {
            return;
        }
        if (location > 255) {
            error(line, column, "label is outside the 256-byte address space");
            pending_labels.clear();
            return;
        }
        for (const auto& name : pending_labels) {
            define_symbol(name, location, kind, size, line, column);
        }
        pending_labels.clear();
    }

    void add_item(Item item) {
        high_water = std::max(high_water, item.address + item.size);
        items.push_back(std::move(item));
    }

    bool parse_directive(const std::string& mnemonic,
                         const std::vector<std::string>& operands, int line,
                         int column);
    bool parse_instruction_line(const std::string& mnemonic,
                                const std::vector<std::string>& operands,
                                int line, int column);
    bool parse_line(const std::string& raw, int line);
    void pass1();
    void pass2();
    void build_listing();
    void run();
};

bool AssemblyState::parse_directive(
    const std::string& mnemonic, const std::vector<std::string>& operands,
    int line, int column) {
    if (mnemonic == ".ORG" || mnemonic == ".ENTRY") {
        if (!pending_labels.empty()) {
            error(line, column, "label has no code or data before " + mnemonic);
            pending_labels.clear();
        }
        if (operands.size() != 1) {
            error(line, column, mnemonic + " needs one expression");
            return false;
        }
        const auto value = evaluate(operands[0], line, column);
        if (!value.has_value()) {
            return false;
        }
        if (*value < 0 || *value > 255) {
            error(line, column, mnemonic + " value must be 0..255");
            return false;
        }
        if (mnemonic == ".ORG") {
            if (*value < location) {
                error(line, column, ".org cannot move backwards");
                return false;
            }
            location = static_cast<int>(*value);
        } else {
            entry_directive = static_cast<int>(*value);
        }
        return true;
    }

    if (mnemonic == ".EQU") {
        if (!pending_labels.empty()) {
            error(line, column, "label has no code or data before .equ");
            pending_labels.clear();
        }
        if (operands.size() != 2) {
            error(line, column, ".equ needs a name and an expression");
            return false;
        }
        const std::string name = trim(operands[0]);
        if (!is_identifier(name)) {
            error(line, column, "invalid .equ name");
            return false;
        }
        const auto value = evaluate(operands[1], line, column);
        if (!value.has_value()) {
            return false;
        }
        if (*value < std::numeric_limits<std::int32_t>::min() ||
            *value > std::numeric_limits<std::int32_t>::max()) {
            error(line, column, ".equ value is out of 32-bit range");
            return false;
        }
        define_symbol(name, static_cast<std::int32_t>(*value),
                      SymbolKind::Const, 0, line, column);
        return true;
    }

    if (mnemonic == ".BYTE" || mnemonic == ".WORD") {
        if (operands.empty()) {
            error(line, column, mnemonic + " needs at least one expression");
            return false;
        }
        const int unit = mnemonic == ".BYTE" ? 1 : 2;
        const int size = static_cast<int>(operands.size()) * unit;
        if (location + size > 256) {
            error(line, column, "data does not fit in 256 bytes");
            return false;
        }
        flush_labels(SymbolKind::Data, static_cast<std::uint16_t>(size), line,
                     column);
        Item item;
        item.kind = mnemonic == ".BYTE" ? ItemKind::Bytes : ItemKind::Words;
        item.line = line;
        item.address = location;
        item.size = size;
        item.exprs = operands;
        add_item(std::move(item));
        location += size;
        return true;
    }

    if (mnemonic == ".ASCII" || mnemonic == ".ASCIZ") {
        if (operands.empty()) {
            error(line, column, mnemonic + " needs at least one string");
            return false;
        }
        std::vector<std::string> strings;
        std::size_t total = mnemonic == ".ASCIZ" ? 1 : 0;
        for (const auto& operand : operands) {
            std::string decoded;
            std::string parse_error;
            if (!parse_string_literal(operand, decoded, parse_error)) {
                error(line, column, parse_error);
                return false;
            }
            total += decoded.size();
            strings.push_back(std::move(decoded));
        }
        if (total > 256 ||
            location + static_cast<int>(total) > 256) {
            error(line, column, "string does not fit in 256 bytes");
            return false;
        }
        flush_labels(SymbolKind::Data, static_cast<std::uint16_t>(total),
                     line, column);
        Item item;
        item.kind = mnemonic == ".ASCII" ? ItemKind::Ascii : ItemKind::Asciz;
        item.line = line;
        item.address = location;
        item.size = static_cast<int>(total);
        item.strings = std::move(strings);
        add_item(std::move(item));
        location += static_cast<int>(total);
        return true;
    }

    if (mnemonic == ".SPACE" || mnemonic == ".ZERO") {
        if (operands.size() != 1) {
            error(line, column, mnemonic + " needs one count");
            return false;
        }
        const auto value = evaluate(operands[0], line, column);
        if (!value.has_value()) {
            return false;
        }
        if (*value < 0 || *value > 256) {
            error(line, column, mnemonic + " count must be 0..256");
            return false;
        }
        const int size = static_cast<int>(*value);
        if (location + size > 256) {
            error(line, column, "reserved space does not fit in 256 bytes");
            return false;
        }
        flush_labels(SymbolKind::Data, static_cast<std::uint16_t>(size), line,
                     column);
        Item item;
        item.kind = ItemKind::Space;
        item.line = line;
        item.address = location;
        item.size = size;
        add_item(std::move(item));
        location += size;
        return true;
    }

    error(line, column, "unknown directive '" + mnemonic + "'");
    return false;
}

bool AssemblyState::parse_instruction_line(
    const std::string& mnemonic, const std::vector<std::string>& operands,
    int line, int column) {
    const ParsedInstruction parsed = parse_instruction(mnemonic, operands);
    if (!parsed.ok) {
        error(line, column, parsed.error);
        return false;
    }
    const OpcodeInfo* info = info_for_op(parsed.op);
    if (info == nullptr) {
        error(line, column, "internal error: unknown opcode");
        return false;
    }
    const int size = info->size;
    if (size == 2 && location == 255) {
        error(line, column,
              "two-byte instruction cannot start at 0xFF");
        return false;
    }
    if (location + size > 256) {
        error(line, column, "instruction does not fit in 256 bytes");
        return false;
    }
    flush_labels(SymbolKind::Code, 0, line, column);
    Item item;
    item.kind = ItemKind::Instruction;
    item.line = line;
    item.address = location;
    item.size = size;
    item.op = parsed.op;
    item.form = parsed.form;
    item.expr = parsed.expr;
    add_item(std::move(item));
    if (first_instruction < 0) {
        first_instruction = location;
    }
    location += size;
    return true;
}

bool AssemblyState::parse_line(const std::string& raw, int line) {
    const std::string without_comment = strip_comment(raw);
    std::string rest = trim(without_comment);
    if (rest.empty()) {
        return true;
    }

    std::string label;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        const char value = rest[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (value == '\\' && (in_string || in_char)) {
            escaped = true;
            continue;
        }
        if (value == '"' && !in_char) {
            in_string = !in_string;
            continue;
        }
        if (value == '\'' && !in_string) {
            in_char = !in_char;
            continue;
        }
        if (value == ':' && !in_string && !in_char) {
            label = trim(std::string_view(rest).substr(0, i));
            rest = trim(std::string_view(rest).substr(i + 1));
            break;
        }
    }
    if (!label.empty()) {
        if (!is_identifier(label)) {
            error(line, 1, "invalid label name '" + label + "'");
            return false;
        }
        pending_labels.push_back(std::move(label));
    }
    if (rest.empty()) {
        return true;
    }

    std::size_t mnemonic_end = 0;
    while (mnemonic_end < rest.size() &&
           std::isspace(static_cast<unsigned char>(rest[mnemonic_end])) == 0) {
        ++mnemonic_end;
    }
    const std::string mnemonic = rest.substr(0, mnemonic_end);
    const std::string operand_text = trim(rest.substr(mnemonic_end));
    const OperandSplit split = split_operands(operand_text);
    if (!split.ok) {
        error(line, 1, split.error);
        return false;
    }

    if (mnemonic.front() == '.') {
        return parse_directive(upper(mnemonic), split.parts, line, 1);
    }
    return parse_instruction_line(mnemonic, split.parts, line, 1);
}

void AssemblyState::pass1() {
    for (std::size_t i = 0; i < source_lines.size(); ++i) {
        parse_line(source_lines[i], static_cast<int>(i) + 1);
    }
    if (!pending_labels.empty()) {
        error(static_cast<int>(source_lines.size()),
              "label has no code or data");
        pending_labels.clear();
    }
    if (first_instruction < 0) {
        error(1, "no instructions to assemble");
        return;
    }
    if (location > 256) {
        error(1, "image exceeds 256 bytes");
        return;
    }
    if (entry_directive.has_value()) {
        result.image.entry = static_cast<std::uint8_t>(*entry_directive);
    } else {
        result.image.entry = static_cast<std::uint8_t>(first_instruction);
    }
    result.image.image_high = static_cast<std::uint16_t>(high_water);
    result.image.sources.clear();
    if (options.include_source) {
        result.image.sources.push_back(
            SourceFile{options.source_name, result.source_text});
    }
    for (const auto& name : symbol_order) {
        result.image.symbols.push_back(symbols.at(name));
    }
    const int headroom = 256 - high_water;
    if (headroom < 16) {
        warning(1, "stack headroom is only " + std::to_string(headroom) +
                       " bytes");
    }
}

void AssemblyState::pass2() {
    auto& image = result.image;
    image.ram.fill(0);
    image.code_map.fill(0);
    image.lines.clear();
    result.code_bytes = 0;
    result.data_bytes = 0;

    for (const auto& item : items) {
        if (item.kind == ItemKind::Instruction) {
            const OpcodeInfo* info = info_for_op(item.op);
            if (info == nullptr) {
                error(item.line, 1, "internal error: unknown opcode");
                continue;
            }
            std::uint8_t operand = 0;
            if (item.form != Form::None) {
                const auto value = evaluate(item.expr, item.line, 1);
                if (!value.has_value()) {
                    continue;
                }
                if (item.form == Form::Target) {
                    if (*value < 0 || *value > 255) {
                        error(item.line, 1, "branch target must be 0..255");
                        continue;
                    }
                } else {
                    if (*value < -128 || *value > 255) {
                        error(item.line, 1,
                              "operand must be -128..255");
                        continue;
                    }
                }
                operand = static_cast<std::uint8_t>(*value & 0xFF);
            }
            image.ram[item.address] = info->opcode;
            if (info->size == 2) {
                image.ram[item.address + 1] = operand;
            }
            image.mark_code(static_cast<std::uint8_t>(item.address));
            image.lines.push_back(SourceLineRow{
                static_cast<std::uint8_t>(item.address), 0,
                static_cast<std::uint16_t>(item.line)});
            result.code_bytes += static_cast<std::uint32_t>(info->size);
            continue;
        }

        int cursor = item.address;
        switch (item.kind) {
            case ItemKind::Bytes:
                for (const auto& expression : item.exprs) {
                    const auto value = evaluate(expression, item.line, 1);
                    if (!value.has_value()) {
                        continue;
                    }
                    if (*value < -128 || *value > 255) {
                        error(item.line, 1, ".byte value must be -128..255");
                        continue;
                    }
                    image.ram[cursor++] =
                        static_cast<std::uint8_t>(*value & 0xFF);
                    ++result.data_bytes;
                }
                break;
            case ItemKind::Words:
                for (const auto& expression : item.exprs) {
                    const auto value = evaluate(expression, item.line, 1);
                    if (!value.has_value()) {
                        continue;
                    }
                    if (*value < -32768 || *value > 65535) {
                        error(item.line, 1, ".word value must be 16-bit");
                        continue;
                    }
                    const auto word = static_cast<std::uint16_t>(*value);
                    image.ram[cursor++] =
                        static_cast<std::uint8_t>(word & 0xFF);
                    image.ram[cursor++] =
                        static_cast<std::uint8_t>((word >> 8) & 0xFF);
                    result.data_bytes += 2;
                }
                break;
            case ItemKind::Ascii:
            case ItemKind::Asciz:
                for (const auto& text : item.strings) {
                    for (const char value : text) {
                        image.ram[cursor++] =
                            static_cast<std::uint8_t>(value);
                        ++result.data_bytes;
                    }
                }
                if (item.kind == ItemKind::Asciz) {
                    image.ram[cursor++] = 0;
                    ++result.data_bytes;
                }
                break;
            case ItemKind::Space:
                result.data_bytes += static_cast<std::uint32_t>(item.size);
                break;
            case ItemKind::Instruction:
                break;
        }
    }

    if (!image.is_code(image.entry)) {
        warning(1, "entry address is not an instruction");
    }
}

void AssemblyState::build_listing() {
    std::ostringstream out;
    out << "ADDR  BYTES     SOURCE\n";
    for (const auto& item : items) {
        out << std::right << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << item.address << "  ";
        std::string bytes;
        if (item.kind != ItemKind::Space) {
            for (int i = 0; i < item.size; ++i) {
                if (!bytes.empty()) {
                    bytes.push_back(' ');
                }
                const std::uint8_t value =
                    result.image.ram[static_cast<std::size_t>(item.address + i)];
                constexpr char digits[] = "0123456789ABCDEF";
                bytes.push_back(digits[(value >> 4) & 0x0F]);
                bytes.push_back(digits[value & 0x0F]);
            }
        }
        out << std::left << std::setfill(' ') << std::setw(12) << bytes
            << "  ";
        if (item.line > 0 &&
            static_cast<std::size_t>(item.line) <= source_lines.size()) {
            out << source_lines[static_cast<std::size_t>(item.line - 1)];
        }
        out << '\n';
    }
    out << "\nSymbols:\n";
    for (const auto& name : symbol_order) {
        const Symbol& symbol = symbols.at(name);
        out << "  " << std::right << std::hex << std::uppercase
            << std::setfill('0') << std::setw(4)
            << (symbol.value & 0xFFFF) << ' ';
        if (symbol.kind == SymbolKind::Code) {
            out << "code ";
        } else if (symbol.kind == SymbolKind::Data) {
            out << "data ";
        } else {
            out << "const";
        }
        out << ' ' << name;
        if (symbol.kind == SymbolKind::Data) {
            out << " (" << std::dec << symbol.size << " bytes)";
        }
        out << '\n';
    }
    const int headroom = 256 - high_water;
    out << "\nImage: " << std::dec << result.code_bytes << " code bytes, "
        << result.data_bytes << " data bytes, image_high=" << high_water
        << ", stack headroom=" << headroom << '\n';
    result.listing = out.str();
}

void AssemblyState::run() {
    result.source_name = options.source_name;
    result.source_text = normalize_source(source);
    source_lines = split_lines(result.source_text);
    result.image.entry = 0;
    result.image.image_high = 0;
    pass1();
    if (had_error) {
        result.ok = false;
        result.diagnostics = diagnostics;
        return;
    }
    pass2();
    build_listing();
    result.diagnostics = diagnostics;
    result.ok = !had_error;
}

}  // namespace

AsmResult assemble(std::string_view source, const AsmOptions& options) {
    AssemblyState state;
    state.options = options;
    state.source = std::string(source);
    state.run();
    return std::move(state.result);
}

AsmResult assemble_file(const std::filesystem::path& path,
                        const AsmOptions& options) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        AsmResult result;
        result.source_name = options.source_name;
        result.diagnostics.push_back(
            AsmDiagnostic{options.source_name, 0, 0,
                          "cannot open source file", false});
        return result;
    }
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    AsmOptions actual = options;
    if (actual.source_name == "<input>") {
        actual.source_name = path.string();
    }
    return assemble(text, actual);
}

}  // namespace kr8
