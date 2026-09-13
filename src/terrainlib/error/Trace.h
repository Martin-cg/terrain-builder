#pragma once

#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace alp {

// A point an error passed through: where it originated, was propagated or was given context.
struct Frame {
    // What was being done at this point, absent where the error originated or was only propagated.
    std::optional<std::string> message = std::nullopt;

    // Where in the source this point is.
    std::source_location location;
};

// The frames an error collected, innermost first.
class Trace {
public:
    // Starts a trace where the error originated.
    explicit Trace(const std::source_location origin) {
        this->_frames.push_back(Frame{.location = origin});
    }

    void push(Frame frame) {
        this->_frames.push_back(std::move(frame));
    }

    // The frames, innermost first.
    [[nodiscard]]
    std::span<const Frame> frames() const {
        return this->_frames;
    }

private:
    std::vector<Frame> _frames;
};

} // namespace alp
