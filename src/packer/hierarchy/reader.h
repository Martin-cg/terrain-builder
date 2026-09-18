#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <libassert/assert.hpp>

#include "PageId.h"
#include "format.h"
#include "numeric/checked_cast.h"
#include "quantize.h"

namespace packer::hierarchy {

// A node of the same page.
struct LocalChild {
    uint16_t node = 0;

    auto operator<=>(const LocalChild &) const = default;
};

// A child as read back from a page.
struct Child {
    using Target = std::variant<LocalChild, ExternalChild, LeafChild>;

    GridBox box;
    float min_error = 0.0f;
    float max_parent_error = 0.0f;
    Target target;

    bool operator==(const Child &) const = default;
};

// Reads a hierarchy page node by node, in page order.
class PageReader {
public:
    explicit PageReader(EncodedPage page)
        : _page(std::move(page)),
          _pending_frames(this->_page.root_frames.begin(), this->_page.root_frames.end()),
          _next_local_node_index(this->_page.root_frames.size()) {}

    // The next node's children, decoded, or nothing once every node has been read.
    // The children stay valid until the next call.
    [[nodiscard]] std::optional<std::span<const Child>> read_next_node() {
        if (this->_node_index == this->_page.child_counts.size()) {
            return std::nullopt;
        }
        return this->read_node();
    }

    // The children of every node not read yet, in page order.
    [[nodiscard]] std::vector<std::vector<Child>> read_all_nodes() {
        std::vector<std::vector<Child>> nodes;
        while (const std::optional<std::span<const Child>> children = this->read_next_node()) {
            nodes.emplace_back(std::from_range, *children);
        }
        return nodes;
    }

private:
    [[nodiscard]] std::span<const Child> read_node() {
        // Nodes take their frames in page order, the roots' first.
        DEBUG_ASSERT(!this->_pending_frames.empty());
        const Frame frame = this->_pending_frames.front();
        this->_pending_frames.pop_front();

        const uint8_t count = this->_page.child_counts[this->_node_index];
        this->_node_index++;

        this->_children.clear();
        for (uint8_t i = 0; i < count; i++) {
            this->_children.push_back(this->read_child(frame));
        }
        return std::span<const Child>(this->_children);
    }

    [[nodiscard]] Child read_child(const Frame &frame) {
        DEBUG_ASSERT(this->_child_offset < this->_page.children.size());
        const EncodedChild &record = this->_page.children[this->_child_offset];
        this->_child_offset++;

        const GridBox box = dequantize_box(frame, record.box);
        return Child{.box = box, .min_error = record.min_error, .max_parent_error = record.max_parent_error, .target = this->read_target(record.kind, box)};
    }

    [[nodiscard]] Child::Target read_target(const ChildKind kind, const GridBox &box) {
        switch (kind) {
        case ChildKind::Local: {
            const LocalChild local{.node = debug_checked_cast<uint16_t>(this->_next_local_node_index)};
            this->_next_local_node_index++;
            this->_pending_frames.push_back(derive_frame(box));
            return local;
        }
        case ChildKind::External: {
            const PageId page = this->read_page(this->_page.page_slots, this->_reference_offset);
            const uint16_t root = read_at(this->_page.local_indices, this->_reference_offset);
            this->_reference_offset++;
            return ExternalChild{.page = page, .root = root};
        }
        case ChildKind::Leaf: {
            const PageId page = this->read_page(this->_page.page_slots, this->_reference_offset);
            const uint16_t cluster = read_at(this->_page.local_indices, this->_reference_offset);
            const PageId texture_page = this->read_page(this->_page.texture_page_slots, this->_texture_reference_offset);
            const uint16_t texture = read_at(this->_page.texture_local_indices, this->_texture_reference_offset);
            this->_reference_offset++;
            this->_texture_reference_offset++;
            return LeafChild{.page = page, .cluster = cluster, .texture_page = texture_page, .texture = texture};
        }
        }
        UNREACHABLE("unknown child kind");
    }

    // The entry of `list` at `index`.
    [[nodiscard]] static uint16_t read_at(const std::span<const uint16_t> list, const size_t index) {
        DEBUG_ASSERT(index < list.size());
        return list[index];
    }

    // The page named by the slot in `slots` at `index`.
    [[nodiscard]] PageId read_page(const std::span<const uint16_t> slots, const size_t index) const {
        const uint16_t slot = read_at(slots, index);
        DEBUG_ASSERT(slot < this->_page.pages.size());
        return this->_page.pages[slot];
    }

    EncodedPage _page;
    // The frames of the nodes still to be read, in page order.
    std::deque<Frame> _pending_frames;
    // The children of the node read last.
    std::vector<Child> _children;
    size_t _node_index = 0;
    size_t _child_offset = 0;
    size_t _reference_offset = 0;
    size_t _texture_reference_offset = 0;
    size_t _next_local_node_index = 0;
};

}
