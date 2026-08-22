#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "containers/spsc.h"

// Merges the sorted streams arriving on K channels into one sorted stream.
template <typename T, size_t K, typename Compare>
class MergingReceiver {
public:
    MergingReceiver(std::array<spsc::Receiver<T>, K> channels, Compare compare)
        : _sources(to_sources(std::move(channels), std::make_index_sequence<K>{})),
          _compare(std::move(compare)) {
    }

    // Takes the smallest value still to come, or nothing once every source has ended.
    // Blocks until each source has either offered a value or ended.
    [[nodiscard]]
    std::optional<T> pop() {
        for (Source &source : this->_sources) {
            if (source.ready.empty()) {
                this->refill(source);
            }
        }

        auto pending = this->_sources | std::views::filter([](const Source &source) {
            return !source.ready.empty();
        });

        const auto best = std::ranges::min_element(
            pending,
            this->_compare,
            [](const Source &source) -> const T & {
                return source.ready.front();
            });

        if (best == pending.end()) {
            return std::nullopt;
        }

        Source &source = *best;
        T value = source.ready.front();
        source.ready = source.ready.subspan(1);
        return value;
    }

private:
    // How many values are taken from a source at once.
    static constexpr size_t prefetch_count = 64;

    struct Source {
        spsc::Receiver<T> channel;
        std::array<T, prefetch_count> buffer{};
        std::span<T> ready{};
    };

    template <size_t... index>
    static std::array<Source, K> to_sources(
        std::array<spsc::Receiver<T>, K> channels, std::index_sequence<index...>) {
        return {Source{.channel = std::move(channels[index])}...};
    }

    // Takes the next values of a source, blocking until it has some or has ended.
    void refill(Source &source) {
        const size_t taken = source.channel.pop_batch(source.buffer);
        source.ready = std::span(source.buffer).first(taken);
    }

    std::array<Source, K> _sources;
    Compare _compare;
};

// Opens a channel per source, and the merge that consumes them.
template <typename T, size_t K, typename Compare = std::less<>>
std::pair<std::array<spsc::Sender<T>, K>, MergingReceiver<T, K, Compare>> make_merging_channels(
    const size_t capacity, Compare compare = {}) {
    std::vector<std::pair<spsc::Sender<T>, spsc::Receiver<T>>> opened;
    opened.reserve(K);
    for (size_t source = 0; source < K; source++) {
        opened.push_back(spsc::make_channel<T>(capacity));
    }

    return [&]<size_t... index>(std::index_sequence<index...>) {
        return std::pair{
            std::array{std::move(opened[index].first)...},
            MergingReceiver<T, K, Compare>(
                std::array{std::move(opened[index].second)...}, std::move(compare)),
        };
    }(std::make_index_sequence<K>{});
}
