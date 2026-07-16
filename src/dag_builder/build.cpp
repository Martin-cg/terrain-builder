#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <tbb/concurrent_vector.h>

#include "build.h"
#include "centroids.h"
#include "cluster.h"
#include "clusterize.h"
#include "compact.h"
#include "encoded.h"
#include "int_math.h"
#include "log.h"
#include "merge/clusterings.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/IdRect.h"
#include "octree/OddLevelShifted.h"
#include "octree/Space.h"
#include "octree/Storage.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"
#include "ProgressIndicator.h"
#include "partition.h"
#include "simplify.h"
#include "slice.h"
#include "range_utils.h"
#include "storage.h"
#include "thread_safe_storage.h"
#include "utils.h"
#include "vertex_lock.h"
#include "parallel.h"
#include "ContinuationMode.h"

namespace dag {

namespace {

// Load a mesh from storage and clusterize it.
std::optional<Clustering> load_and_clusterize_mesh(
    const octree::MeshStorage &storage,
    const octree::Id &id) {
    const auto result = storage.load(id);

    if (!result) {
        if (result.error() != mesh::io::LoadMeshErrorKind::FileNotFound) {
            LOG_ERROR("Failed to read node {}: {}", id, result.error());
        }
        return std::nullopt;
    }

    const mesh::Simple &mesh = result.value();

    LOG_DEBUG(
        "Clustering mesh for node {} with {} vertices and {} triangles",
        id,
        mesh.vertex_count(),
        mesh.face_count());

    return clusterize(mesh);
}

// A clustering and a mapping from its clusters to the original input clusters that were merged to produce it.
struct LodResult {
    Clustering clustering;
    std::vector<std::vector<uint32_t>> child_map;
};

// Run the full LOD pipeline on a clustering: partition, simplify, re-clusterize, and build child map.
LodResult build_lod(
    const Clustering &clusters,
    const BuildOptions &options,
    const radix::geometry::Aabb3d &node_bounds,
    const std::span<const uint8_t> seam_vertex_mask) {
    const Partitioning partitioning = create_partitioning(clusters, PartitionOptions{
                                                                        .clusters_per_partition = options.clusters_per_partition});
    // Construct new clusters and generate new textures.
    Clustering clustering = apply_partitioning(clusters, partitioning, options.uv_unwrap_algorithm);
    // Create partition to cluster map
    std::vector<std::vector<uint32_t>> partition_to_clusters(partitioning.partition_count);
    for (const auto [cluster_index, partition_index] : enumerate(partitioning.cluster_partitions)) {
        partition_to_clusters[partition_index].push_back(cluster_index);
    }

    // Trim textures
    trim_textures_inplace(clustering);

    // Find vertices to lock
    const std::vector<uint8_t> vertex_lock = find_vertices_to_lock(clustering, node_bounds, seam_vertex_mask);

    // Convert relative_target_error (a fraction of the node bounds) to an absolute
    // error; both targets stay optional so simplify() can apply its own default.
    std::optional<float> absolute_target_error;
    if (options.relative_target_error) {
        const double node_extent = glm::compMax(node_bounds.size());
        absolute_target_error = static_cast<float>(options.relative_target_error.value() * node_extent);
    }
    const SimplifyOptions simplify_options{
        .target_ratio = options.target_ratio,
        .absolute_target_error = absolute_target_error,
        .vertex_lock = VertexLock::mask(vertex_lock),
        .error_mode = ErrorMode::Add,
        .preserve_cluster_count = true
    };
    clustering = simplify(clustering, simplify_options);
    remove_unused_vertices_inplace(clustering);

    // Split each cluster into roughly 4 parts
    auto result = clusterize(clustering);

    // Build child map. Simplification preserves the cluster count, so each final
    // cluster's backward mapping index is its partition index.
    std::vector<std::vector<uint32_t>> child_map(result.clustering.cluster_count());
    for (const auto [final_index, partition_index] : enumerate(result.backward_mapping)) {
        child_map[final_index] = partition_to_clusters[partition_index];
    }

    return {std::move(result.clustering), std::move(child_map)};
}

// Dependencies shared across the whole dag building pipeline.
struct BuildContext {
    const octree::IndexedMeshStorage &input_storage;
    ThreadSafeStorage<octree::IndexedDagStorage> output_storage;
    const BuildOptions &options;
    const octree::Space &space;
    const octree::OddLevelShifted &shifted_space;
    const radix::geometry::Aabb3d &root_bounds;
};

// Dag clusters loaded whole (so excluded clusters can still act as seam context), plus
// which of them match inner_filter and where each one came from.
struct InnerSources {
    std::vector<Clustering> clusterings;
    std::vector<dag::Id> cluster_sources;
    std::vector<uint32_t> selected_indices; // positions of matching clusters once concatenated
    uint32_t cluster_count = 0;
};

InnerSources load_inner_sources(
    const std::vector<octree::Id> &inner_ids,
    const RegionFilter &inner_filter,
    const BuildContext &ctx) {
    InnerSources result;

    for (const octree::Id &id : inner_ids) {
        auto inner_node = ctx.output_storage.load(id);
        if (!inner_node) {
            LOG_WARN("Failed to load DAG node {}, skipping", id);
            continue;
        }
        Clustering clustering = std::move(inner_node.value().clustering);

        // Assign canonical cluster ids, which double as positions once concatenated.
        for (auto &[cluster_index, cluster] : enumerate(clustering.clusters)) {
            cluster.id = result.cluster_count + cluster_index;
            result.cluster_sources.emplace_back(id, cluster_index);
        }

        for (const uint32_t local_index : find_clusters_matching(clustering, inner_filter)) {
            result.selected_indices.push_back(result.cluster_count + local_index);
        }
        result.cluster_count += clustering.cluster_count();

        result.clusterings.push_back(std::move(clustering));
    }

    return result;
}

// Input tiles loaded whole (so excluded clusters can still act as seam context against dag
// content), plus which of them match input_filter once concatenated.
struct InputSources {
    std::vector<Clustering> clusterings;
    std::vector<uint32_t> selected_indices; // positions of matching clusters once concatenated
    uint32_t cluster_count = 0;
};

InputSources load_input_sources(
    const std::vector<octree::Id> &input_ids,
    const RegionFilter &input_filter,
    const BuildContext &ctx) {
    InputSources result;

    for (const octree::Id &id : input_ids) {
        auto opt = load_and_clusterize_mesh(ctx.input_storage, id);
        if (!opt) {
            LOG_WARN("Failed to load or clusterize {}, skipping", id);
            continue;
        }
        Clustering clustering = std::move(*opt);

        for (const uint32_t local_index : find_clusters_matching(clustering, input_filter)) {
            result.selected_indices.push_back(result.cluster_count + local_index);
        }
        result.cluster_count += clustering.cluster_count();

        result.clusterings.push_back(std::move(clustering));
    }

    return result;
}

// Marks vertices on dag_selected clusters that are also referenced by an unselected one --
// an excluded dag cluster or an input cluster.
std::vector<uint8_t> compute_seam_vertex_mask(
    const Clustering &combined,
    const std::span<const uint32_t> inner_selected_indices,
    const VertexMap &inner_vertex_map) {
    const std::vector<bool> combined_seam_mask = find_seam_vertices(combined, inner_selected_indices);

    std::vector<uint8_t> seam_vertex_mask(inner_vertex_map.new_vertex_count(), 0);
    for (const auto &[combined_index, inner_index] : enumerate(inner_vertex_map.forward())) {
        if (inner_index != VertexMap::invalid_index && combined_seam_mask[combined_index]) {
            seam_vertex_mask[inner_index] = 1;
        }
    }
    return seam_vertex_mask;
}

// Stitches simplified dag-derived clusters back together with the input clusters that
// were merged alongside them. A second merge is unavoidable here since simplify() moved
// the dag clusters into a new vertex space.
dag::ClusterBatch stitch_input_and_simplified(
    Clustering input_clusters,
    Clustering simplified,
    std::vector<std::vector<dag::Id>> child_id_map,
    const double epsilon) {
    if (input_clusters.is_empty()) {
        return {std::move(simplified), std::move(child_id_map)};
    }

    const uint32_t input_cluster_count = input_clusters.cluster_count();
    std::vector<Clustering> sources{std::move(input_clusters), std::move(simplified)};
    Clustering combined = merge_clusterings(sources, epsilon);

    std::vector<std::vector<dag::Id>> combined_child_map(combined.cluster_count());
    for (const auto &[index, children] : enumerate(child_id_map)) {
        combined_child_map[input_cluster_count + index] = std::move(children);
    }

    return {std::move(combined), std::move(combined_child_map)};
}

// Compute epsilon value for merging clusters based on the size of the node bounds.
double compute_epsilon(const radix::geometry::Aabb3d &bounds) {
    return glm::compAdd(bounds.size()) / 10000;
}

// Merge dag children and input tiles into one vertex space so seams -- whether the far
// side of one is an excluded dag cluster or an input cluster -- can be found by cluster
// adjacency instead of proximity to node_bounds, then simplify the dag-selected part.
std::optional<dag::ClusterBatch> load_and_simplify_sources(
    const std::vector<octree::Id> &inner_ids,
    const std::vector<octree::Id> &input_ids,
    const RegionFilter &inner_filter,
    const RegionFilter &input_filter,
    const radix::geometry::Aabb3d &node_bounds,
    const BuildContext &ctx) {
    InnerSources inner = load_inner_sources(inner_ids, inner_filter, ctx);
    InputSources input = load_input_sources(input_ids, input_filter, ctx);

    if (inner.selected_indices.empty() && input.selected_indices.empty()) {
        return std::nullopt;
    }

    // Offset input's selected positions past the dag clusters they're concatenated after.
    const std::vector<uint32_t> input_selected_indices = transform_vector(input.selected_indices, [&](const uint32_t local_index) {
        return inner.cluster_count + local_index;
    });

    const double epsilon = compute_epsilon(node_bounds);

    std::vector<Clustering> all_clusterings = std::move(inner.clusterings);
    all_clusterings.insert(all_clusterings.end(), std::make_move_iterator(input.clusterings.begin()), std::make_move_iterator(input.clusterings.end()));
    const Clustering combined = merge_clusterings(all_clusterings, epsilon);

    Clustering input_clusters;
    if (!input_selected_indices.empty()) {
        input_clusters = slice_clusters(combined, input_selected_indices);
    }

    // No dag content to simplify: the merge above already reconciled input with itself.
    if (inner.selected_indices.empty()) {
        return dag::ClusterBatch::make_leaves(std::move(input_clusters));
    }

    const auto [inner_clusters, inner_vertex_map] = slice_clusters_with_map(combined, inner.selected_indices);
    const std::vector<uint8_t> seam_vertex_mask = compute_seam_vertex_mask(combined, inner.selected_indices, inner_vertex_map);

    // Run nanite-style grouping, simplification, splitting.
    auto [simplified, child_map] = build_lod(inner_clusters, ctx.options, node_bounds, seam_vertex_mask);

    // Create map from cluster index to child cluster id.
    auto child_id_map = transform_vector(child_map, [&](const auto &children) {
        return transform_vector(children, [&](const uint32_t inner_cluster_index) {
            const uint32_t source_index = inner_clusters.clusters[inner_cluster_index].id;
            return inner.cluster_sources[source_index];
        });
    });

    return stitch_input_and_simplified(std::move(input_clusters), std::move(simplified), std::move(child_id_map), epsilon);
}

// Build a single target node by loading input clusters (preserved as-is) and
// DAG children (inner, run through simplification pipeline), then combining both.
std::optional<dag::ClusterBatch> build_node(
    const octree::Id &target_id,
    const std::vector<octree::Id> &input_ids,
    const std::vector<octree::Id> &inner_ids,
    const BuildContext &ctx) {
    LOG_DEBUG("Building node {} ({} inputs, {} inner nodes)", target_id, input_ids.size(), inner_ids.size());

    const auto node_bounds = ctx.shifted_space.get_node_bounds(target_id);

    // Prepare filter to only include clusters inside the target_id bounds.
    // In CurrentAndCoarser mode, input regions represented by the relevant DAG nodes are excluded as well.
    RegionFilter input_filter;
    input_filter.include = {node_bounds};
    if (ctx.options.include_mode == IncludeMode::CurrentAndCoarser) {
        for (const octree::Id &inner_id : inner_ids) {
            input_filter.exclude.push_back(ctx.shifted_space.get_node_bounds(inner_id));
        }
    }
    RegionFilter inner_filter;
    inner_filter.include = {node_bounds};

    std::optional<dag::ClusterBatch> result = load_and_simplify_sources(inner_ids, input_ids, inner_filter, input_filter, node_bounds, ctx);
    if (!result) {
        LOG_WARN("No valid clusters for node {}, skipping", target_id);
        return std::nullopt;
    }

    LOG_INFO("Finished node {} with {} clusters", target_id, result->clustering.cluster_count());

    return result;
}

struct LevelWorkplan {
    std::vector<octree::Id> targets;
    std::unordered_map<octree::Id, std::vector<octree::Id>> input_sources;
    std::unordered_map<octree::Id, std::vector<octree::Id>> inner_nodes;
};

// Return all input nodes from input_by_level that spatially overlap the given target node.
std::vector<octree::Id> find_relevant_input_nodes(
    const octree::Id &target_id,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const octree::OddLevelShifted &shifted_space,
    const octree::Space &space,
    const IncludeMode include_mode) {
    std::vector<octree::Id> result;
    if (include_mode == IncludeMode::CurrentOnly) {
        // Include same-level inputs that intersect with the target_id in the shifted tree.
        const std::vector<octree::Id> &input_nodes = input_by_level[target_id.level()];
        octree::IdRect standard_nodes = shifted_space.find_intersecting_standard_nodes(target_id);
        result.reserve(standard_nodes.size());
        for (const octree::Id& input_id : input_nodes) {
            if (standard_nodes.contains(input_id)) {
                result.push_back(input_id);
            }
        }
    } else if (include_mode == IncludeMode::CurrentAndCoarser) {
        // Include inputs at same or coarser levels whose bounds intersect target_id in the shifted tree.
        const radix::geometry::Aabb3d target_bounds = shifted_space.get_node_bounds(target_id);
        for (uint32_t level = 0; level <= target_id.level(); level++) {
            const std::vector<octree::Id> &input_nodes = input_by_level[level];
            for (const octree::Id& input_id : input_nodes) {
                const radix::geometry::Aabb3d input_bounds = space.get_node_bounds(input_id);
                if (radix::geometry::intersect(target_bounds, input_bounds)) {
                    result.push_back(input_id);
                }
            }
        }
    }
    return result;
}

// Return the already-built DAG nodes that are children of the given target node.
std::vector<octree::Id> find_relevant_inner_nodes(
    const octree::Id &target_id,
    const std::unordered_set<octree::Id> &prev_level_built,
    const octree::OddLevelShifted &shifted_space) {
    std::vector<octree::Id> result;
    for (const octree::Id &child : shifted_space.get_intersecting_nodes_on_level(target_id, target_id.level()+1)) {
        if (prev_level_built.contains(child)) {
            result.push_back(child);
        }
    }
    return result;
}

// Discover all shifted-space target nodes for one level from inputs and already-built children.
std::unordered_set<octree::Id> find_nodes_to_build_on_level(
    const uint32_t level,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const std::unordered_set<octree::Id> &prev_level_built,
    const BuildContext &ctx) {
    std::unordered_set<octree::Id> target_set;

    // Consider input nodes at this level
    const std::span<const octree::Id> level_input_ids = input_by_level[level];
    for (const octree::Id &input_id : level_input_ids) {
        const octree::IdRect shifted_nodes = ctx.shifted_space.find_intersecting_nodes_for_standard_id(input_id);
        for (const octree::Id &shifted_id : shifted_nodes) {
            target_set.insert(shifted_id);
        }
    }

    // Consider parents of previously built nodes
    for (const octree::Id &child : prev_level_built) {
        const octree::IdRect parents = ctx.shifted_space.get_intersecting_nodes_on_level(child, level);
        for (const octree::Id &parent : parents) {
            target_set.insert(parent);
        }
    }

    return target_set;
}

// Build a workplan for a single level, determining which target nodes to build and which input and DAG nodes are relevant for each target.
LevelWorkplan build_level_workplan(
    const uint32_t level,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const std::unordered_set<octree::Id> &prev_level_built,
    const BuildContext &ctx) {
    std::unordered_set<octree::Id> target_set = find_nodes_to_build_on_level(level, input_by_level, prev_level_built, ctx);

    std::unordered_map<octree::Id, std::vector<octree::Id>> input_sources;
    std::unordered_map<octree::Id, std::vector<octree::Id>> inner_nodes;
    for (const octree::Id &target : target_set) {
        input_sources[target] = find_relevant_input_nodes(target, input_by_level, ctx.shifted_space, ctx.space, ctx.options.include_mode);
        inner_nodes[target] = find_relevant_inner_nodes(target, prev_level_built, ctx.shifted_space);
    }

    return {
        to_vector(target_set),
        std::move(input_sources),
        std::move(inner_nodes)};
}

// Pre-filter input nodes to only those that intersect the target root bounds.
std::vector<std::vector<octree::Id>> gather_relevant_input_leaves(
    const octree::IndexMap &index,
    const octree::Space &space,
    const radix::geometry::Aabb3d &root_bounds)
{
    const auto start = space.find_smallest_node_encompassing_bounds(root_bounds)
        .value_or(octree::Id::root());
    std::vector<std::vector<octree::Id>> result(octree::Id::max_level() + 1);
    octree::traverse(
        index,
        [&](const octree::Id &id, const octree::NodeStatus status) {
            if (status == octree::NodeStatus::Leaf && radix::geometry::intersect(root_bounds, space.get_node_bounds(id))) {
                result[id.level()].push_back(id);
            }
        },
        [&](const octree::Id &id) {
            return radix::geometry::intersect(root_bounds, space.get_node_bounds(id));
        },
        start);
    return result;
}

// Find the maximum level in input_by_level that has any nodes.
std::optional<uint32_t> find_max_input_level(const std::vector<std::vector<octree::Id>> &input_by_level) {
    std::optional<uint32_t> max_level;
    for (uint32_t level = 0; level < input_by_level.size(); level++) {
        if (!input_by_level[level].empty()) {
            max_level = level;
        }
    }
    return max_level;
}

} // namespace

// Build a single level of the DAG, returning the set of nodes that were built.
std::unordered_set<octree::Id> build_level(
    const uint32_t level,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const std::unordered_set<octree::Id> &prev_level_built,
    const BuildContext &ctx) {
    auto [targets, input_sources, inner_nodes] =
        build_level_workplan(level, input_by_level, prev_level_built, ctx);

    if (targets.empty()) {
        return {};
    }

    LOG_INFO("Building level {} ({} targets)", level, targets.size());

    std::unordered_set<octree::Id> already_built;
    if (ctx.options.continuation_mode != ContinuationMode::Overwrite) {
        for (const octree::Id &target : targets) {
            if (ctx.output_storage.has(target)) {
                already_built.insert(target);
            }
        }
    }

    if (ctx.options.continuation_mode == ContinuationMode::Error && already_built.empty()) {
        LOG_ERROR_AND_EXIT("Found some of target nodes already in built in the output directory, use --resume or --overwrite");
    }

    // Initialize debug storage if requested (contains .glb meshes)
    std::optional<octree::MeshStorage> debug_storage;
    if (ctx.options.write_debug_meshes) {
        debug_storage = octree::open_folder(
            ctx.output_storage.base_path().string() + "-debug",
            false,
            octree::OpenOptions{.preferred_extension_with_dot = ".glb"});
    }

    tbb::concurrent_vector<octree::Id> saved_ids;

    ProgressIndicator progress(targets.size());
    auto progress_thread = progress.start_monitoring();

    parallel_foreach(targets, [&](const octree::Id &target) {
        if (already_built.contains(target)) {
            progress.task_finished();
            return;
        }

        const auto inner_ids = find_value(inner_nodes, target).value_or({});
        const auto target_input_ids = find_value(input_sources, target).value_or({});

        auto result = build_node(target, target_input_ids, inner_ids, ctx);
        if (result) {
            const auto save_result = ctx.output_storage.save(target, *result);
            if (save_result) {
                if (debug_storage) {
                    debug_storage->save(target, clustering_to_mesh(result->clustering));
                }
                saved_ids.push_back(target);
            } else {
                LOG_ERROR("Failed to save node {}: {}", target, save_result.error());
            }
        }
        progress.task_finished();
    }, ctx.options.parallelize);

    progress_thread.join();

    std::unordered_set<octree::Id> built = std::move(already_built);
    for (const octree::Id &id : saved_ids) {
        built.insert(id);
    }

    return built;
}

// Builds the DAG from input_storage into output_storage, restricted to levels within level_range.
// Iterates octree levels from finest to coarsest, simplifying and re-clustering geometry at each
// level from its children.
void build_levels(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options,
    const AnyRange<uint32_t> &level_range) {
    const octree::OddLevelShifted shifted_space = octree::OddLevelShifted::earth();
    const octree::Space space = octree::Space::earth();
    const octree::Id root_node = options.root_node;
    const auto root_bounds = shifted_space.get_node_bounds_with_children(root_node);
    auto input_by_level = gather_relevant_input_leaves(input_storage.index(), space, root_bounds);

    // Find the maximum input level that has any nodes
    auto max_input_level_opt = find_max_input_level(input_by_level);
    if (!max_input_level_opt.has_value()) {
        LOG_WARN("No input nodes found for root {}", root_node);
        return;
    }
    const uint32_t max_input_level = max_input_level_opt.value();

    const Range<uint32_t> valid_range{root_node.level(), max_input_level + 1};
    const Range<uint32_t> range = valid_range.intersect(level_range).to_range(max_input_level + 1);
    if (range.is_empty()) {
        LOG_WARN("Requested level range does not overlap with buildable levels {}-{}", root_node.level(), max_input_level);
        return;
    }

    // Seed prev_level_built with any already-built nodes one level finer than the first level.
    // TODO: use hierachical lookup here based on root_bounds and IdRect
    std::unordered_set<octree::Id> prev_level_built;
    for (const auto &[id, status] : output_storage.index()) {
        if (id.level() == range.end && status != octree::NodeStatus::Virtual) {
            prev_level_built.insert(id);
        }
    }

    BuildContext ctx{input_storage, ThreadSafeStorage(std::move(output_storage)), options, space, shifted_space, root_bounds};

    for (uint32_t level = range.end; level-- > range.start;) {
        prev_level_built = build_level(
            level,
            input_by_level,
            prev_level_built,
            ctx);
    }

    output_storage = std::move(ctx.output_storage).release();
}

// Builds the complete DAG from input_storage into output_storage.
void build_full(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options) {
    build_levels(input_storage, output_storage, options, RangeFull{});
}

} // namespace dag
