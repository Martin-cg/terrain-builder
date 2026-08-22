#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <deque>
#include <functional>
#include <ranges>
#include <utility>

#include <libassert/assert.hpp>

template <typename T, size_t K, typename Sink, typename Compare>
class MergingWriter {
public:
    MergingWriter(Sink sink, Compare compare)
        : _sink(std::move(sink)),
          _compare(std::move(compare)) {
    }

    void push(const size_t source, T value) {
        DEBUG_ASSERT(source < K);
        DEBUG_ASSERT(!this->_completed[source]);

        this->_queued[source].push_back(std::move(value));
        this->drain();
    }

    void complete(const size_t source) {
        DEBUG_ASSERT(source < K);
        DEBUG_ASSERT(!this->_completed[source]);

        this->_completed[source] = true;
        this->drain();
    }

private:
    void drain() {
        while (this->ready()) {
            const size_t source = this->next_source();

            T value = std::move(this->_queued[source].front());
            this->_queued[source].pop_front();

            this->_sink(std::move(value));
        }
    }
    
    [[nodiscard]]
    bool ready() const {
        const auto sources = std::views::iota(size_t{0}, K);

        const bool none_blocked = std::ranges::none_of(
            sources,
            [this](const size_t source) {
                return this->_queued[source].empty() && !this->_completed[source];
            });
        const bool any_queued = std::ranges::any_of(
            sources,
            [this](const size_t source) {
                return !this->_queued[source].empty();
            });

        return none_blocked && any_queued;
    }

    [[nodiscard]]
    size_t next_source() const {
        auto sources =
            std::views::iota(size_t{0}, K) | std::views::filter([this](const size_t source) {
                return !this->_queued[source].empty();
            });

        const auto best = std::ranges::min_element(
            sources,
            this->_compare,
            [this](const size_t source) -> const T & {
                return this->_queued[source].front();
            });

        DEBUG_ASSERT(best != sources.end());
        return *best;
    }

    std::array<std::deque<T>, K> _queued;
    std::array<bool, K> _completed{};

    Sink _sink;
    Compare _compare;
};
