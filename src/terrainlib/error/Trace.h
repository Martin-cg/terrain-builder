#pragma once

#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace alp {

// A point an error passed through after it originated: where it was propagated or was given context.
struct Frame {
    // What was being done at this point, absent where the error was only propagated.
    std::optional<std::string> message = std::nullopt;

    // Where in the source this point is.
    std::source_location location;
};

// Where an error originated and the frames it collected after that.
class Trace {
public:
    // Starts a trace for an error that originates at `origin`, with no frames yet.
    explicit Trace(const std::source_location origin) : _origin(origin) {}

    void push(Frame frame) {
        this->_frames.push_back(std::move(frame));
    }

    // Where the error originated.
    [[nodiscard]]
    std::source_location origin() const {
        return this->_origin;
    }

    // The frames added after the origin, innermost first.
    [[nodiscard]]
    std::span<const Frame> frames() const {
        return this->_frames;
    }

private:
    std::source_location _origin;
    std::vector<Frame> _frames;
};

} // namespace alp
