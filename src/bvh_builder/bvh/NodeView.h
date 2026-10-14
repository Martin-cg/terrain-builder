#pragma once

#include <span>
#include <utility>
#include <variant>

#include <libassert/assert.hpp>

#include "variant_utils.h"

#include "bvh/NodeHeader.h"
#include "bvh/Reference.h"
#include "dag/ClusterInfo.h"

namespace bvh {

class NodeView {
public:
    using Children = std::span<const Reference>;
    using Clusters = std::span<const dag::ClusterInfo>;
    using Contents = std::variant<Children, Clusters>;

    NodeView(const NodeHeader &header, const Contents contents)
        : _header(header), _contents(contents) {}

    const NodeHeader &header() const {
        return this->_header;
    }

    bool is_leaf() const {
        return std::holds_alternative<Clusters>(this->_contents);
    }

    bool is_inner() const {
        return !this->is_leaf();
    }

    Children children() const {
        DEBUG_ASSERT(this->is_inner());
        return std::get<Children>(this->_contents);
    }

    Clusters clusters() const {
        DEBUG_ASSERT(this->is_leaf());
        return std::get<Clusters>(this->_contents);
    }

    // Calls whichever of `visitors` matches what the node holds.
    template <typename... F>
    decltype(auto) visit(F &&...visitors) const {
        return match(this->_contents, std::forward<F>(visitors)...);
    }

private:
    NodeHeader _header;
    Contents _contents;
};

}
