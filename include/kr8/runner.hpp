#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "kr8/image.hpp"

namespace kr8 {

struct RunOptions {
    std::optional<std::uint8_t> entry;
    std::uint64_t max_instructions = 10'000'000;
    bool trace = false;
    bool read_stdin = false;
    std::vector<std::uint8_t> input;
};

enum class RunStatus {
    Halted,
    Break,
    Fault,
    Budget,
};

struct RunResult {
    RunStatus status = RunStatus::Budget;
    std::string message;
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;
    std::string output;
    std::string trace;
    std::array<std::uint8_t, 256> ram{};

    bool ok() const noexcept {
        return status == RunStatus::Halted || status == RunStatus::Break;
    }
};

RunResult run_image(const Image& image, const RunOptions& options = {});

}  // namespace kr8
