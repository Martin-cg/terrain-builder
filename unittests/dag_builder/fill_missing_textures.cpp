#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "fill_missing_textures.h"

TEST_CASE("fill_missing_textures_inplace gives a cluster without uvs one uv per vertex", "[fill_missing_textures]") {
    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    clustering.textures.add(cv::Mat::zeros(4, 4, CV_8UC3));

    Cluster bare;
    bare.vertex_indices = {0, 1, 2};
    bare.local_triangles = {{0, 1, 2}};
    bare.texture_id = 0;

    Cluster mapped = bare;
    mapped.uvs = {{0.25, 0.0}, {1.0, 0.5}, {0.0, 1.0}};

    clustering.clusters = {bare, mapped};
    fill_missing_textures_inplace(clustering);

    CHECK(clustering.clusters[0].uvs.size() == clustering.clusters[0].vertex_count());
    CHECK(clustering.clusters[1].uvs == mapped.uvs);
}

TEST_CASE("fill_missing_textures_inplace gives an untextured cluster one of the existing textures", "[fill_missing_textures]") {
    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    clustering.textures.add(cv::Mat::zeros(4, 4, CV_8UC3));
    clustering.textures.add(cv::Mat::zeros(2, 2, CV_8UC3));

    Cluster untextured;
    untextured.vertex_indices = {0, 1, 2};
    untextured.local_triangles = {{0, 1, 2}};
    untextured.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};

    Cluster textured = untextured;
    textured.texture_id = 1;

    clustering.clusters = {untextured, textured};
    fill_missing_textures_inplace(clustering);

    CHECK(clustering.textures.size() == 2);
    REQUIRE(clustering.clusters[0].is_textured());
    CHECK(clustering.clusters[0].texture_id.value() < clustering.textures.size());
    CHECK(clustering.clusters[1].texture_id == 1u);
}

TEST_CASE("fill_missing_textures_inplace adds a texture to a clustering without any", "[fill_missing_textures]") {
    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};

    Cluster untextured;
    untextured.vertex_indices = {0, 1, 2};
    untextured.local_triangles = {{0, 1, 2}};
    untextured.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};

    clustering.clusters = {untextured};
    fill_missing_textures_inplace(clustering);

    REQUIRE(clustering.clusters[0].is_textured());
    CHECK(clustering.clusters[0].texture_id.value() < clustering.textures.size());
}