#pragma once

#include <memory>
#include <vector>

#include <ogr_spatialref.h>

#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "srs.h"
#include "srs/error.h"

namespace srs {

namespace detail {

inline MissingTransformation make_missing_transformation(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs) {
    return MissingTransformation{.source = friendly_name(source_srs), .target = friendly_name(target_srs)};
}

struct TransformationCacheEntry {
    OGRSpatialReference source;
    OGRSpatialReference target;
    std::shared_ptr<OGRCoordinateTransformation> transform;
};

} // namespace detail

[[nodiscard]]
inline alp::Result<std::unique_ptr<OGRCoordinateTransformation>, MissingTransformation> uncached_transformation(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs) {
    std::unique_ptr<OGRCoordinateTransformation> transform(OGRCreateCoordinateTransformation(&source_srs, &target_srs));
    if (!transform) {
        return alp::make_unexpected(detail::make_missing_transformation(source_srs, target_srs));
    }
    return transform;
}

// Returns a transformation from `source_srs` to `target_srs` that belongs to the calling thread and must only be used on it.
// A thread gets the same transformation for every later request of the same pair.
// Fails when no transformation exists from `source_srs` to `target_srs`.
[[nodiscard]]
inline alp::Result<std::shared_ptr<OGRCoordinateTransformation>, MissingTransformation> transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs) {
    thread_local std::vector<detail::TransformationCacheEntry> cache;

    // Check forward
    for (auto &entry : cache) {
        if (entry.source.IsSame(&source_srs) && entry.target.IsSame(&target_srs)) {
            return entry.transform;
        }
    }

    // Check inverse 
    for (auto &entry : cache) {
        if (entry.source.IsSame(&target_srs) && entry.target.IsSame(&source_srs)) {
            std::shared_ptr<OGRCoordinateTransformation> inverse(entry.transform->GetInverse());
            if (!inverse) {
                return alp::make_unexpected(detail::make_missing_transformation(source_srs, target_srs));
            }
            cache.push_back({source_srs, target_srs, inverse});
            return inverse;
        }
    }

    // Not found
    ALP_TRY(auto transform, uncached_transformation(source_srs, target_srs));
    std::shared_ptr<OGRCoordinateTransformation> shared_transform = std::move(transform);
    cache.push_back({source_srs, target_srs, shared_transform});
    return shared_transform;
}

}
