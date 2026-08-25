#pragma once

#include <cstddef>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// Runs one copy of a body per worker, keeping what each one returns.
template <typename Result>
class WorkerGroup {
public:
    template <typename Body>
    WorkerGroup(const size_t count, Body &&body) : _results(count) {
        this->_workers.reserve(count);
        for (size_t worker = 0; worker < count; worker++) {
            // Every thread takes its own copy, since the caller's body only has to
            // outlive this constructor.
            this->_workers.emplace_back([this, body, worker]() {
                this->_results[worker] = body();
            });
        }
    }

    // The threads hold `this`, so the group has to stay where it was started.
    WorkerGroup(const WorkerGroup &) = delete;
    WorkerGroup &operator=(const WorkerGroup &) = delete;

    // Waits for every worker and hands back their results, in worker order.
    [[nodiscard]]
    std::span<const Result> join() {
        this->_workers.clear();
        return this->_results;
    }

private:
    std::vector<Result> _results;
    std::vector<std::jthread> _workers;
};

template <typename Body>
WorkerGroup(size_t, Body &&) -> WorkerGroup<std::invoke_result_t<Body &>>;
