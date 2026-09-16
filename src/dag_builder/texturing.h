#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "atlas/bake_cluster_texture.h"
#include "bake/bake.h"
#include "cluster.h"
#include "cluster_adjacency.h"
#include "cluster_views.h"
#include "containers/StampSet.h"
#include "compact.h"
#include "enumerate.h"
#include "log.h"
#include "Partitioning.h"
#include "range_utils.h"
#include "texture_reuse.h"
#include "texture_sizing.h"
#include "utils.h"
#include "uv/atlas.h"

// Charts are packed for roughly this much of their atlas.
inline constexpr double ESTIMATED_ATLAS_COVERAGE = 0.75;

// Resolution to pack an atlas at, so its gutter lands near the scale the bake uses.
[[nodiscard]]
inline uint32_t compute_unwrap_resolution(const double demand) {
    return std::max(int_ceil(std::sqrt(demand / ESTIMATED_ATLAS_COVERAGE)), 1u);
}

// Whether any of the clusters carries a texture.
inline bool any_source_texture(const Clustering &clustering, const std::span<const uint32_t> cluster_indices) {
    return std::ranges::any_of(cluster_indices, [&](const uint32_t i) {
        return clustering.clusters[i].is_textured();
    });
}

enum class ChartingMode {
    PerCluster, // one atlas per cluster
    PerNode, // one atlas over the full clustering
};

struct TextureOptions {
    uv::AtlasOptions atlas = {};
    bake::Options bake = {};
    bake::Mode bake_mode = bake::ProjectAlongNormal{};
    TextureSizingOptions sizing = {};
    ChartingMode charting = ChartingMode::PerNode;
    bool allow_texture_reuse = true;
    // The colour of the dummy texture a cluster without imagery gets.
    cv::Scalar untextured_color = cv::Scalar::all(0);
};

// Give every untextured cluster a dummy texture, so later stages never meet one.
inline void fill_untextured_clusters_inplace(Clustering &clustering, const cv::Scalar &color) {
    std::optional<uint32_t> dummy_id;

    for (Cluster &cluster : clustering.clusters) {
        if (cluster.is_textured()) {
            continue;
        }
        // Added on first use, so a fully textured clustering gains nothing.
        if (!dummy_id.has_value()) {
            dummy_id = clustering.textures.add(cv::Mat(1, 1, CV_8UC3, color));
        }
        cluster.texture_id = dummy_id.value();

        // Every uv addresses the one texel the dummy holds, so zeros lose nothing.
        if (!cluster.has_uvs()) {
            cluster.uvs.assign(cluster.vertex_count(), glm::dvec2(0));
        }
    }
}

// Texels each cluster should carry, from how densely its sources were textured.
[[nodiscard]]
inline std::vector<double> compute_cluster_texel_demands(
    const Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const TextureSizingOptions &options) {
    std::vector<double> demands(merged.cluster_count(), 0.0);

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        const double source_density = compute_source_texel_density(source, partition_to_clusters.segment(index));
        demands[index] = compute_target_texel_density(options, source_density) * cluster.triangle_count();
    }

    return demands;
}

// The clusters needing an unwrap: those whose sources carry a texture, but which the merge
// could not carry one into.
[[nodiscard]]
inline std::vector<uint32_t> find_clusters_to_unwrap(
    const Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters) {
    std::vector<uint32_t> to_unwrap;

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        if (cluster.is_textured() || !any_source_texture(source, partition_to_clusters.segment(index))) {
            continue;
        }
        to_unwrap.push_back(index);
    }

    return to_unwrap;
}

// Width over height of a texture.
[[nodiscard]]
inline double compute_aspect(const glm::uvec2 size) {
    return double(size.x) / size.y;
}

// Size one texture for the clusters that will share it.
[[nodiscard]]
inline BakePlan size_texture(
    const Clustering &merged,
    const std::span<const double> demands,
    std::vector<uint32_t> clusters,
    const double aspect) {
    const TextureDemand demand{
        .texels = sum(clusters, [&](const uint32_t index) {
            return demands[index];
        }),
        .utilization = sum(clusters, [&](const uint32_t index) {
            return compute_utilization(merged.clusters[index]);
        })
    };
    return BakePlan{
        .clusters = std::move(clusters),
        .size = compute_target_size(demand, aspect)
    };
}

// Collect the clusters that kept a carried-in texture, one entry per texture.
[[nodiscard]]
inline std::vector<BakePlan> size_inherited_textures(
    const Clustering &merged,
    const std::span<const double> demands) {
    constexpr uint32_t no_entry = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> entry_of_texture(merged.textures.size(), no_entry);
    std::vector<std::vector<uint32_t>> clusters_per_texture;
    std::vector<uint32_t> texture_ids;

    for (const auto [index, cluster] : enumerate(merged.clusters)) {
        if (!cluster.is_textured()) {
            continue;
        }
        const uint32_t texture_id = cluster.texture_id.value();
        uint32_t &entry = entry_of_texture[texture_id];
        if (entry == no_entry) {
            entry = clusters_per_texture.size();
            clusters_per_texture.emplace_back();
            texture_ids.push_back(texture_id);
        }
        clusters_per_texture[entry].push_back(index);
    }

    std::vector<BakePlan> plans;
    plans.reserve(texture_ids.size());
    for (const auto [entry, texture_id] : enumerate(texture_ids)) {
        const glm::uvec2 current_size = get_texture_size(merged.textures[texture_id]);
        BakePlan plan = size_texture(merged, demands, std::move(clusters_per_texture[entry]), compute_aspect(current_size));
        plan.inherited_id = texture_id;
        // Upscaling adds no detail, so never plan more than the texture already holds.
        plan.size = glm::min(plan.size, current_size);
        plans.push_back(std::move(plan));
    }

    return plans;
}

// Chart each cluster on its own, so every chart boundary is also a cluster boundary.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_per_cluster(
    Clustering &merged,
    const std::span<const uint32_t> to_unwrap,
    const std::span<const double> demands,
    const uv::AtlasOptions &options) {
    std::vector<BakePlan> plans;

    for (const uint32_t index : to_unwrap) {
        const Cluster &cluster = merged.clusters[index];

        // Perform unwrap at approximately the final resolution
        const std::vector<glm::dvec3> positions = merged.get_cluster_positions(index);
        uv::Atlas atlas = uv::build_atlas(cluster.local_triangles, positions, compute_unwrap_resolution(demands[index]), options);
        if (atlas.uvs.empty()) {
            LOG_WARN("Cluster {} could not be unwrapped, leaving it untextured", index);
            continue;
        }

        const double aspect = atlas.aspect();

        // Apply new uvs and duplicated seam vertices
        merged.clusters[index] = apply_atlas(cluster, std::move(atlas));

        plans.push_back(size_texture(merged, demands, {index}, aspect));
    }

    return plans;
}

// Chart every cluster as one surface, so charts may span cluster boundaries.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_per_node(
    Clustering &merged,
    const std::span<const uint32_t> to_unwrap,
    const std::span<const double> demands,
    const uv::AtlasOptions &options) {
    std::vector<glm::uvec3> triangles;
    for (const uint32_t index : to_unwrap) {
        const Cluster &cluster = merged.clusters[index];
        for (const glm::uvec3 &local : cluster.local_triangles) {
            triangles.push_back(cluster.global_triangle(local));
        }
    }

    // Perform unwrap at approximately the final resolution
    const double demanded_texels = sum(to_unwrap, [&](const uint32_t index) {
        return demands[index];
    });
    const uv::Atlas atlas = uv::build_atlas(triangles, merged.positions, compute_unwrap_resolution(demanded_texels), options);
    if (atlas.uvs.empty()) {
        LOG_WARN("Node of {} clusters could not be unwrapped, leaving it untextured", to_unwrap.size());
        return {};
    }

    // Apply new uvs and duplicated seam vertices
    apply_node_atlas(merged, to_unwrap, atlas);

    std::vector<uint32_t> clusters(to_unwrap.begin(), to_unwrap.end());

    std::vector<BakePlan> plans;
    plans.push_back(size_texture(merged, demands, std::move(clusters), atlas.aspect()));
    return plans;
}

// Give every cluster uvs to address its texture with.
[[nodiscard]]
inline std::vector<BakePlan> unwrap_clusters(
    Clustering &merged,
    const std::span<const uint32_t> to_unwrap,
    const std::span<const double> demands,
    const TextureOptions &options) {
    if (to_unwrap.empty()) {
        return {};
    }

    switch (options.charting) {
    case ChartingMode::PerCluster:
        return unwrap_per_cluster(merged, to_unwrap, demands, options.atlas);
    case ChartingMode::PerNode:
        return unwrap_per_node(merged, to_unwrap, demands, options.atlas);
    default:
        UNREACHABLE();
    }
}

// Resize every carried-in texture to what its clusters were budgeted.
inline void rescale_inherited_textures(Clustering &merged, const std::span<const BakePlan> textures) {
    for (const BakePlan &plan : textures) {
        if (!plan.inherited_id.has_value()) {
            continue;
        }
        cv::Mat &texture = merged.textures[plan.inherited_id.value()];
        if (get_texture_size(texture) != plan.size) {
            texture = rescale_texture(texture, plan.size);
        }
    }
}

// The clusters a plan bakes from: the ones that merged into it, and the ones bordering those.
[[nodiscard]]
inline std::vector<uint32_t> collect_source_clusters(
    const std::span<const uint32_t> plan_clusters,
    const PartitionToClusters &partition_to_clusters,
    const ClusterAdjacency &adjacency,
    StampSet &visited) {
    visited.reset(adjacency.segment_count());

    std::vector<uint32_t> source_clusters;
    const auto visit = [&](const uint32_t cluster) {
        if (visited.insert(cluster)) {
            source_clusters.push_back(cluster);
        }
    };

    for (const uint32_t index : plan_clusters) {
        for (const uint32_t cluster : partition_to_clusters.segment(index)) {
            visit(cluster);
            // Output triangles on a cluster border can lean onto surface that merged into a different one.
            for (const uint32_t neighbour : adjacency.segment(cluster)) {
                visit(neighbour);
            }
        }
    }

    return source_clusters;
}

// Run every planned bake, at the size the node budget left it.
inline void bake_node_textures(
    Clustering &merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const std::span<const BakePlan> plans,
    const bake::Options &options,
    const bake::Mode &mode) {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(source);
    StampSet visited(source.cluster_count());

    for (const BakePlan &plan : plans) {
        if (plan.inherited_id.has_value()) {
            continue;
        }

        const std::vector<uint32_t> source_clusters =
            collect_source_clusters(plan.clusters, partition_to_clusters, adjacency, visited);

        const cv::Mat baked = bake::render_texture(
            ClusteringView{merged, plan.clusters}, ClusteringView{source, source_clusters}, plan.size, options, mode);
        const uint32_t texture_id = merged.textures.add(baked);

        for (const uint32_t index : plan.clusters) {
            merged.clusters[index].texture_id = texture_id;
        }
    }
}

// Unwrap and bake a texture for every merged cluster whose sources had one.
[[nodiscard]]
inline Clustering texture_clusters(
    Clustering merged,
    const Clustering &source,
    const PartitionToClusters &partition_to_clusters,
    const TextureOptions &options) {
    DEBUG_ASSERT(merged.cluster_count() == partition_to_clusters.segment_count());

    // Carry over the source texture wherever the merged clusters can keep addressing it
    if (options.allow_texture_reuse) {
        inherit_shared_textures(merged, source, partition_to_clusters);
    }

    const std::vector<double> demands = compute_cluster_texel_demands(merged, source, partition_to_clusters, options.sizing);
    const std::vector<uint32_t> to_unwrap = find_clusters_to_unwrap(merged, source, partition_to_clusters);

    // No texture is resized or baked until every cluster in the node has asked for a size
    std::vector<BakePlan> textures = size_inherited_textures(merged, demands);
    for (BakePlan &unwrapped : unwrap_clusters(merged, to_unwrap, demands, options)) {
        textures.push_back(std::move(unwrapped));
    }
    clamp_to_max_side(textures, options.sizing.max_texture_side);
    rescale_to_fit_budget(textures, options.sizing.max_node_texels);

    // Realize at new size
    rescale_inherited_textures(merged, textures);
    bake_node_textures(merged, source, partition_to_clusters, textures, options.bake, options.bake_mode);

    remove_unused_textures_inplace(merged);
    return merged;
}
