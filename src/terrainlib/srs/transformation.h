#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include <ogr_spatialref.h>

namespace srs {

namespace {
struct TransformationCacheEntry {
    OGRSpatialReference source;
    OGRSpatialReference target;
    std::shared_ptr<OGRCoordinateTransformation> transform;
};
}

inline std::unique_ptr<OGRCoordinateTransformation> uncached_transformation(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs) {
    std::unique_ptr<OGRCoordinateTransformation> transform(OGRCreateCoordinateTransformation(&source_srs, &target_srs));
    if (!transform) {
        throw std::runtime_error("Couldn't create SRS transformation");
    }
    return transform;
}

inline std::shared_ptr<OGRCoordinateTransformation> transformation(const OGRSpatialReference& source_srs, const OGRSpatialReference& target_srs) {
    thread_local std::vector<TransformationCacheEntry> cache;

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
                throw std::runtime_error("Couldn't create inverse transformation");
            }
            cache.push_back({source_srs, target_srs, inverse});
            return inverse;
        }
    }

    // Not found
    auto transform = uncached_transformation(source_srs, target_srs);
    std::shared_ptr<OGRCoordinateTransformation> shared_transform = std::move(transform);
    cache.push_back({source_srs, target_srs, shared_transform});
    return shared_transform;
}

}
