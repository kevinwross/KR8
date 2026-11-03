#include "kr8/machine.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace kr8 {

Machine::Machine() {
    cpu_.set_out_handler([this](std::uint8_t value) {
        console_.push_back(static_cast<char>(value));
    });
    cpu_.set_in_handler([this]() -> std::optional<std::uint8_t> {
        if (!input_.empty()) {
            const std::uint8_t value = input_.front();
            input_.pop_front();
            return value;
        }
        if (fallback_in_) {
            return fallback_in_();
        }
        return std::nullopt;
    });
}

void Machine::load(std::span<const std::uint8_t> image, std::uint8_t entry,
                   std::uint16_t stack_floor) {
    memory_.load(image);
    entry_ = entry;
    stack_floor_ = stack_floor;
    reset();
}

void Machine::reset() {
    cpu_.reset(entry_, stack_floor_);
    console_.clear();
    input_.clear();
}

StepResult Machine::step() {
    return cpu_.step(memory_);
}

}  // namespace kr8
