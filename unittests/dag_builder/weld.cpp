#include "../catch2_helpers.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "cluster.h"
#include "geometry/geometry.h"
#include "merge/weld.h"
#include "mesh/TriangleSoup.h"
#include "range_utils.h"
#include "utils.h"

namespace {

Cluster make_cluster(std::vector<uint32_t> vertex_indices, std::vector<glm::uvec3> local_triangles) {
    Cluster cluster;
    cluster.id = 0;
    cluster.vertex_indices = std::move(vertex_indices);
    cluster.local_triangles = std::move(local_triangles);
    return cluster;
}

std::vector<TriangleSoup> to_cluster_soups(const Clustering &clustering) {
    return transform_vector(clustering.clusters, [&](const Cluster &cluster) {
        return to_triangle_soup(materialize_cluster(cluster, clustering.positions));
    });
}
} // namespace

TEST_CASE("weld_clustering welds positions within epsilon and keeps the rest apart", "[merge][weld]") {
    const double epsilon = GENERATE(0.0, 1.0);

    // near is within epsilon of anchor, far is beyond epsilon of both.
    const glm::dvec3 anchor(0.1, 0.0, 0.0);
    const glm::dvec3 near(0.1, 0.0, 0.0);
    const glm::dvec3 far(0.1, -1.1, 0.0);

    Clustering clustering;
    clustering.positions = {
        anchor, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
        near, glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0),
        far, glm::dvec3(5.0, -10.0, 0.0), glm::dvec3(-5.0, -10.0, 0.0),
    };
    clustering.clusters.push_back(make_cluster({0, 1, 2}, {glm::uvec3(0, 1, 2)}));
    clustering.clusters.push_back(make_cluster({3, 4, 5}, {glm::uvec3(0, 1, 2)}));
    clustering.clusters.push_back(make_cluster({6, 7, 8}, {glm::uvec3(0, 1, 2)}));

    const Clustering welded = weld_clustering(clustering, epsilon);

    CHECK_THAT(welded.positions, Catch::Matchers::UnorderedEquals<glm::dvec3>({
        near, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
        glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0),
        far, glm::dvec3(5.0, -10.0, 0.0), glm::dvec3(-5.0, -10.0, 0.0),
    }));
    CHECK(to_cluster_soups(welded) == std::vector<TriangleSoup>{
        {{near, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0)}},
        {{near, glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0)}},
        {{far, glm::dvec3(5.0, -10.0, 0.0), glm::dvec3(-5.0, -10.0, 0.0)}},
    });
}

TEST_CASE("weld_clustering places the welded vertex by averaging or last writer", "[merge][weld]") {
    struct Case {
        bool average_positions;
        glm::dvec3 welded;
    };
    const Case c = GENERATE(
        Case{true, glm::dvec3(0.25, 0.0, 0.0)},
        Case{false, glm::dvec3(0.4, 0.0, 0.0)});

    Clustering clustering;
    clustering.positions = {
        glm::dvec3(0.1, 0.0, 0.0), glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
        glm::dvec3(0.4, 0.0, 0.0), glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0),
    };
    clustering.clusters.push_back(make_cluster({0, 1, 2}, {glm::uvec3(0, 1, 2)}));
    clustering.clusters.push_back(make_cluster({3, 4, 5}, {glm::uvec3(0, 1, 2)}));

    const Clustering welded = weld_clustering(clustering, 1.0, c.average_positions);

    const auto is_close = [](const glm::dvec3 &a, const glm::dvec3 &b) {
        return glm::all(glm::epsilonEqual(a, b, geometry::epsilon<double>()));
    };
    CHECK_THAT(welded.positions, Catch::Matchers::UnorderedRangeEquals(std::vector<glm::dvec3>{
        c.welded, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
        glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0),
    }, is_close));
}

TEST_CASE("weld_clustering keeps both local vertices of a cluster that repeats a position", "[merge][weld]") {
    const double epsilon = GENERATE(0.0, 1.0);
    const glm::dvec3 seam(0.1, 0.0, 0.0);

    Clustering clustering;
    clustering.positions = {
        seam, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
        seam, glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0),
    };
    clustering.clusters.push_back(make_cluster({0, 1, 2, 3, 4, 5}, {glm::uvec3(0, 1, 2), glm::uvec3(3, 4, 5)}));

    const Clustering welded = weld_clustering(clustering, epsilon);

    CHECK_THAT(welded.positions, Catch::Matchers::UnorderedEquals<glm::dvec3>({
        seam, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
        glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0),
    }));
    REQUIRE(welded.clusters.size() == 1);
    CHECK(welded.clusters[0].vertex_count() == 6);
    CHECK(to_cluster_soups(welded) == std::vector<TriangleSoup>{{
        {seam, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0)},
        {seam, glm::dvec3(-10.0, 0.0, 0.0), glm::dvec3(-5.0, 10.0, 0.0)},
    }});
}

TEST_CASE("weld_clustering drops triangles collapsed by welding", "[merge][weld]") {
    const double epsilon = GENERATE(0.0, 1.0);
    const glm::dvec3 duplicate(0.1, 0.0, 0.0);

    Clustering clustering;
    clustering.positions = {duplicate, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0), duplicate};
    clustering.clusters.push_back(make_cluster({0, 1, 2, 3}, {glm::uvec3(0, 1, 2), glm::uvec3(0, 3, 1)}));

    const Clustering welded = weld_clustering(clustering, epsilon);

    CHECK_THAT(welded.positions, Catch::Matchers::UnorderedEquals<glm::dvec3>({
        duplicate, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0),
    }));
    CHECK(to_cluster_soups(welded) == std::vector<TriangleSoup>{
        {{duplicate, glm::dvec3(10.0, 0.0, 0.0), glm::dvec3(5.0, 10.0, 0.0)}},
    });
}

TEST_CASE("weld_clustering rejects a negative or non finite epsilon", "[merge][weld]") {
    const double epsilon = GENERATE(
        -1.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity());

    Clustering clustering;
    clustering.positions = {glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(0.0, 1.0, 0.0)};
    clustering.clusters.push_back(make_cluster({0, 1, 2}, {glm::uvec3(0, 1, 2)}));

    CHECK_THROWS_AS(weld_clustering(clustering, epsilon), std::invalid_argument);
}

TEST_CASE("weld_clustering returns an empty clustering unchanged", "[merge][weld]") {
    const Clustering welded = weld_clustering(Clustering{}, 1.0);

    CHECK(welded.positions.empty());
    CHECK(welded.clusters.empty());
}
