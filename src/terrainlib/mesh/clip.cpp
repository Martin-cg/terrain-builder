#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "build_config.h"
#include "containers/Cow.h"
#include "containers/FixedVector.h"
#include "geometry/AxisAlignedPlane.h"
#include "geometry/bounds.h"
#include "geometry/geometry.h"
#include "hash_utils.h"
#include "log.h"
#include "mesh/cgal.h"
#include "mesh/clip.h"
#include "mesh/convert.h"
#include "mesh/geometry.h"
#include "mesh/connectivity/adjacency.h"
#include "mesh/normalize.h"
#include "mesh/validate.h"
#include "mesh/validate_cgal.h"
#include "polygon/clip_with.h"
#include "polygon/triangulate.h"
#include "variant_utils.h"
#include "vector_utils.h"

namespace mesh {
namespace {

// Where a source triangle lies relative to the bounds.
enum class Placement { Inside, Outside, Partial };

// Checks where `triangle` lies relative to `bounds`.
// Partial if it is neither fully inside nor provably outside.
Placement check_placement_in_bounds(const radix::geometry::Aabb3d &bounds, const std::array<glm::dvec3, 3> &triangle) {
    const bool is_inside = std::ranges::all_of(triangle, [&](const glm::dvec3 &p) { return bounds.contains_inclusive(p); });
    if (is_inside) {
        return Placement::Inside;
    }
    if (!radix::geometry::intersect(bounds, geometry::calculate_bounds(triangle))) {
        return Placement::Outside;
    }
    return Placement::Partial;
}

// A source vertex.
struct Original {
    uint32_t vertex_index;
};

// A cut where a plane crosses a source edge, shared by both triangles on that edge.
struct OnEdge {
    glm::uvec2 edge;
    SignedAxis plane_normal;

    bool operator==(const OnEdge &) const = default;

    struct Hash {
        size_t operator()(const OnEdge &on_edge) const {
            return hash::combine(on_edge.edge, on_edge.plane_normal);
        }
    };
};

// Checks whether the source vertex with `vertex_index` ends `edge`.
bool is_endpoint_of(const uint32_t vertex_index, const glm::uvec2 &edge) {
    return vertex_index == edge.x || vertex_index == edge.y;
}

// A cut inside a source triangle, used by that triangle only.
struct Inside {};

using VertexProvenance = std::variant<Original, OnEdge, Inside>;

// A vertex of a clipped source triangle.
struct ClippedVertex {
    glm::dvec3 position;
    glm::dvec2 uv;
    VertexProvenance provenance;
};

// Clipping a triangle by six planes adds at most one vertex per plane.
constexpr size_t max_clipped_polygon_size = 9;
using ClippedPolygon = FixedVector<ClippedVertex, max_clipped_polygon_size>;

// Identifies the cut by `plane` between `from` and `to` by the source edge both lie on, or as inside the source triangle if they share none.
VertexProvenance make_crossing(const VertexProvenance &from, const VertexProvenance &to, const AxisAlignedPlane &plane) {
    // A polygon edge between a source vertex and a cut lies on a source edge only if that vertex ends it.
    return std::visit(
        overloaded{
            [&](const Original &a, const Original &b) -> VertexProvenance {
                return OnEdge{normalize_edge(glm::uvec2(a.vertex_index, b.vertex_index)), plane.normal};
            },
            [&](const Original &vertex, const OnEdge &on_edge) -> VertexProvenance {
                return is_endpoint_of(vertex.vertex_index, on_edge.edge) ? VertexProvenance(OnEdge{on_edge.edge, plane.normal}) : Inside{};
            },
            [&](const OnEdge &on_edge, const Original &vertex) -> VertexProvenance {
                return is_endpoint_of(vertex.vertex_index, on_edge.edge) ? VertexProvenance(OnEdge{on_edge.edge, plane.normal}) : Inside{};
            },
            [&](const OnEdge &a, const OnEdge &b) -> VertexProvenance {
                if (a.edge == b.edge) {
                    return OnEdge{a.edge, plane.normal};
                }
                return Inside{};
            },
            [](const auto &, const auto &) -> VertexProvenance {
                return Inside{};
            },
        },
        from,
        to);
}

// Clips the vertices of a source triangle against a plane, keeping track of the source vertex or edge each one lies on.
struct TrackedPolicy {
    using Vertex = ClippedVertex;
    using HalfSpace = AxisAlignedPlane;

    // Measures the signed distance of `vertex` from `plane`.
    static double compute_distance(const AxisAlignedPlane &plane, const ClippedVertex &vertex) {
        return geometry::compute_distance(plane, vertex.position);
    }

    // Computes the vertex where the edge from `from` to `to` crosses `plane`, at `t` along it.
    static ClippedVertex compute_crossing(const AxisAlignedPlane &plane, const ClippedVertex &from, const ClippedVertex &to, const double t) {
        // Projecting puts the cut exactly on the bounds, so later planes and neighbouring bounds measure it without rounding.
        const glm::dvec3 interpolated = geometry::lerp(from.position, to.position, t);
        const glm::dvec3 position = geometry::project_onto_plane(interpolated, plane);
        const glm::dvec2 uv = geometry::lerp(from.uv, to.uv, t);
        return ClippedVertex{position, uv, make_crossing(from.provenance, to.provenance, plane)};
    }
};

// Clips the source `triangle` to all `planes`.
ClippedPolygon clip_triangle(const SimpleMesh &mesh, const glm::uvec3 &triangle, const std::array<AxisAlignedPlane, 6> &planes) {
    ClippedPolygon polygon;
    for (uint8_t k = 0; k < 3; k++) {
        const uint32_t vertex_index = triangle[k];
        const glm::dvec2 uv = mesh.has_uvs() ? mesh.uvs[vertex_index] : glm::dvec2(0);
        polygon.push_back(ClippedVertex{mesh.positions[vertex_index], uv, Original{vertex_index}});
    }

    // Each plane adds at most one vertex only while the polygon is convex.
    // Rounding can break that only for vertices within rounding of a plane without lying on it, and then the polygon overflows and throws.
    for (const AxisAlignedPlane &plane : planes) {
        polygon = polygon::clip_with<TrackedPolicy>(polygon, plane);
    }
    return polygon;
}

// The clipped mesh under construction, with every shared vertex added once.
class ClippedMeshBuilder {
public:
    explicit ClippedMeshBuilder(const SimpleMesh &source)
        : _source(source), _clipped_index_of_source(source.vertex_count(), invalid_index) {}

    // Adds a source triangle unchanged.
    void add_triangle(const glm::uvec3 &source_triangle) {
        glm::uvec3 clipped_triangle;
        for (uint8_t k = 0; k < 3; k++) {
            clipped_triangle[k] = this->add_source_vertex(source_triangle[k]);
        }
        this->_triangles.push_back(clipped_triangle);
    }

    // Adds the convex `polygon` as a fan of triangles.
    // Ignores a polygon with fewer than three vertices.
    void add_polygon(const ClippedPolygon &polygon) {
        if (polygon.size() < 3) {
            return;
        }

        // Every vertex is used by the fan, so each can be added up front.
        // Zero-area triangles are kept, since dropping one would leave a gap where it meets the triangles sharing its edges.
        FixedVector<uint32_t, max_clipped_polygon_size> clipped_indices;
        for (const ClippedVertex &vertex : polygon) {
            clipped_indices.push_back(this->add_vertex(vertex));
        }
        polygon::triangulate_fan(clipped_indices, [&](const std::array<uint32_t, 3> &triangle) {
            const glm::uvec3 clipped_triangle(triangle[0], triangle[1], triangle[2]);
            DEBUG_ASSERT(!is_degenerate(clipped_triangle));
            this->_triangles.push_back(clipped_triangle);
        });
    }

    // Takes the clipped mesh.
    SimpleMesh build() && {
        return SimpleMesh(std::move(this->_triangles), std::move(this->_positions), std::move(this->_uvs), this->_source.texture);
    }

private:
    static constexpr uint32_t invalid_index = static_cast<uint32_t>(-1);

    uint32_t add_source_vertex(const uint32_t source_index) {
        uint32_t &clipped_index = this->_clipped_index_of_source[source_index];
        if (clipped_index == invalid_index) {
            clipped_index = this->append_vertex(this->_source.positions[source_index], this->_source.has_uvs() ? this->_source.uvs[source_index] : glm::dvec2(0));
        }
        return clipped_index;
    }

    uint32_t add_vertex(const ClippedVertex &vertex) {
        return match(
            vertex.provenance,
            [&](const Original &original) {
                return this->add_source_vertex(original.vertex_index);
            },
            [&](const OnEdge &on_edge) {
                // A plane crosses an edge at most once, so the edge and plane identify the cut in both triangles sharing the edge.
                const auto [it, inserted] = this->_clipped_index_of_crossing.try_emplace(on_edge, invalid_index);
                if (inserted) {
                    it->second = this->append_vertex(vertex.position, vertex.uv);
                }
                return it->second;
            },
            [&](const Inside &) {
                // No other triangle can share this cut: it lies strictly inside one source triangle, whose polygon stays whole until the fan.
                return this->append_vertex(vertex.position, vertex.uv);
            });
    }

    uint32_t append_vertex(const glm::dvec3 &position, const glm::dvec2 &uv) {
        const uint32_t clipped_index = this->_positions.size();
        this->_positions.push_back(position);
        if (this->_source.has_uvs()) {
            this->_uvs.push_back(uv);
        }
        return clipped_index;
    }

    const SimpleMesh &_source;
    std::vector<uint32_t> _clipped_index_of_source;
    std::unordered_map<OnEdge, uint32_t, OnEdge::Hash> _clipped_index_of_crossing;
    std::vector<glm::uvec3> _triangles;
    std::vector<glm::dvec3> _positions;
    std::vector<glm::dvec2> _uvs;
};

} // namespace

Cow<SimpleMesh> clip_on_bounds(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds) {
    validate(mesh);

    if (mesh.vertex_count() == 0 || mesh.face_count() == 0) {
        return Cow<SimpleMesh>::owned(SimpleMesh());
    }

    if (mesh.has_uvs() && mesh.uvs.size() != mesh.vertex_count()) {
        LOG_ERROR_AND_EXIT("Mesh has Uvs, but the number of Uvs does not match the number of vertices.");
    }

    const std::array<AxisAlignedPlane, 6> planes = geometry::construct_face_planes(bounds);

    ClippedMeshBuilder clipped(mesh);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        const std::array<glm::dvec3, 3> positions = {mesh.positions[triangle.x], mesh.positions[triangle.y], mesh.positions[triangle.z]};
        switch (check_placement_in_bounds(bounds, positions)) {
        case Placement::Inside:
            clipped.add_triangle(triangle);
            break;
        case Placement::Partial:
            clipped.add_polygon(clip_triangle(mesh, triangle, planes));
            break;
        case Placement::Outside:
            break;
        }
    }

    SimpleMesh clipped_mesh = std::move(clipped).build();
    validate(clipped_mesh);
    return Cow<SimpleMesh>::owned(std::move(clipped_mesh));
}

namespace {
template <typename TriangleMesh>
struct HasIntersectionsVisitor : public CGAL::Polygon_mesh_processing::Corefinement::Default_visitor<TriangleMesh> {
    using HalfedgeDescriptor = typename boost::graph_traits<TriangleMesh>::halfedge_descriptor;

    bool has_intersections = false;

    // called when a new intersection point is detected.
    // The intersection is detected using a face of tm_f and an edge of tm_e. 
    void intersection_point_detected(
        size_t /*i_id*/, int /*sdim*/,
        HalfedgeDescriptor /*h_e*/, HalfedgeDescriptor /*h_f*/, 
        const TriangleMesh& /*tm_e*/, const TriangleMesh& /*tm_f*/, bool /*is_target_coplanar*/, bool /*is_source_coplanar*/) {
        this->has_intersections = true;
    }
};
} // namespace

Cow<SimpleMesh> clip_on_bounds_and_cap(const SimpleMesh &mesh, const radix::geometry::Aabb3d &bounds, const bool remesh_planar_patches) {
    ASSERT(!mesh.has_uvs());
    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);

    const CGAL::Iso_cuboid_3<cgal::Kernel> cgal_bounds(
        convert::to_cgal_point<cgal::Kernel>(bounds.min),
        convert::to_cgal_point<cgal::Kernel>(bounds.max));
    HasIntersectionsVisitor<cgal::Mesh> visitor;
    const auto clip_params = CGAL::Polygon_mesh_processing::parameters::clip_volume(true)
        .visitor(visitor);
    const bool success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_bounds, clip_params);
    if (!success) {
        throw std::runtime_error("CGAL::Polygon_mesh_processing::clip failed");
    }
    if (cgal_mesh.number_of_faces() == 0) {
        return Cow<SimpleMesh>::owned(SimpleMesh());
    }
    if (!visitor.has_intersections && cgal_mesh.number_of_faces() == mesh.face_count()) {
        return Cow<SimpleMesh>::borrowed(mesh);
    }

    cgal_mesh.collect_garbage();

    if (!remesh_planar_patches) {
        return Cow<SimpleMesh>::owned(convert::to_simple_mesh(cgal_mesh));
    }
    
    cgal::Mesh remeshed_cgal_mesh;
    const auto remesh_params = CGAL::Polygon_mesh_processing::parameters::cosine_of_maximum_angle(0.999);
    CGAL::Polygon_mesh_processing::remesh_planar_patches(cgal_mesh, remeshed_cgal_mesh, remesh_params);
    remeshed_cgal_mesh.collect_garbage();
    return Cow<SimpleMesh>::owned(convert::to_simple_mesh(remeshed_cgal_mesh));
}

namespace {
template <typename TriangleMesh, typename UvMap>
struct UvInterpolatorVisitor : public CGAL::Polygon_mesh_processing::Corefinement::Default_visitor<TriangleMesh> {
    using HalfedgeDescriptor = typename boost::graph_traits<TriangleMesh>::halfedge_descriptor;
    using VertexDescriptor = typename boost::graph_traits<TriangleMesh>::vertex_descriptor;
    using VertexIndex = typename TriangleMesh::Vertex_index;

    UvMap& uv_map;
    TriangleMesh &mesh;

    struct Vertex {
        glm::dvec3 position;
        glm::dvec2 uv;
    };

    UvInterpolatorVisitor(UvMap& uv_map, TriangleMesh& mesh) : uv_map(uv_map), mesh(mesh) {}

    struct EdgeIntersection {
        Vertex source;
        Vertex target;
    };

    struct FaceIntersection {
        std::array<Vertex, 3> vertices;
    };
    std::vector<std::variant<EdgeIntersection, FaceIntersection>> intersections;

    // called when a new intersection point is detected.
    // The intersection is detected using a face of tm_f and an edge of tm_e. 
    void intersection_point_detected(
        size_t i_id, int /*sdim*/,
        HalfedgeDescriptor h_e, HalfedgeDescriptor h_f, 
        const TriangleMesh& tm_e, const TriangleMesh& tm_f, bool /*is_target_coplanar*/, bool /*is_source_coplanar*/) {
        DEBUG_ASSERT(i_id == intersections.size());
        ALP_UNUSED(i_id);
        if (&mesh == &tm_e) {
            // The edge belongs to the mesh being clipped
            const auto source_vertex = mesh.source(h_e);
            const auto target_vertex = mesh.target(h_e);
            const auto source_position = convert::to_glm_point(mesh.point(source_vertex));
            const auto target_position = convert::to_glm_point(mesh.point(target_vertex));
            const auto source_uv = uv_map[source_vertex];
            const auto target_uv = uv_map[target_vertex];
            intersections.emplace_back(EdgeIntersection {
                Vertex(source_position, source_uv),
                Vertex(target_position, target_uv)
            });
        } else {
            DEBUG_ASSERT(&mesh == &tm_f);
            // The face belongs to the mesh that is being clipping
            std::array<Vertex, 3> triangle;
            unsigned int i = 0;
            for (const cgal::VertexIndex vertex_index : CGAL::vertices_around_face(h_f, tm_f)) {
                ASSERT(i < triangle.size());
                const auto position = convert::to_glm_point(mesh.point(vertex_index));
                const auto uv = uv_map[vertex_index];
                triangle[i] = Vertex(position, uv);
                i++;
            }

            intersections.emplace_back(FaceIntersection {
                triangle
            });
        }
    }

    void new_vertex_added(size_t i_id, VertexDescriptor vh, const TriangleMesh& tm) {
        DEBUG_ASSERT(i_id < intersections.size());
        if (&tm != &mesh) {
            return;
        }

        const glm::dvec3 new_position = convert::to_glm_point(mesh.point(vh));
        const glm::dvec2 new_uv = std::visit([&](const auto& intersection) {
            using Intersection = std::decay_t<decltype(intersection)>;
            if constexpr (std::is_same_v<Intersection, EdgeIntersection>) {
                const double edge_length = glm::distance(intersection.source.position, intersection.target.position);
                const double t = glm::distance(new_position, intersection.source.position) / edge_length;
                return glm::mix(intersection.source.uv, intersection.target.uv, t);
            } else {
                const glm::dvec3 barycentric = geometry::compute_barycentric(
                    new_position,
                    intersection.vertices[0].position,
                    intersection.vertices[1].position,
                    intersection.vertices[2].position
                );
                // return glm::dvec2(1, 1);
                return barycentric[0] * intersection.vertices[0].uv
                    + barycentric[1] * intersection.vertices[1].uv
                    + barycentric[2] * intersection.vertices[2].uv;
            }
        }, intersections[i_id]);
        uv_map[vh] = new_uv;
    }
};
} // namespace

Cow<SimpleMesh> clip_on_mesh(const SimpleMesh &mesh, const SimpleMesh &clip_mesh, const bool keep_inside) {
    // short circuit the empty mesh case
    // this is a common case since we clip the mask in the terrainmerger on the octree node bounds
    // which often results in empty masks
    if (clip_mesh.face_count() == 0) {
        if (keep_inside) {
            return Cow<SimpleMesh>::owned(SimpleMesh());
        } else {
            return Cow<SimpleMesh>::borrowed(mesh);
        }
    }
    
    // If we want to keep everything outside the clip_mesh we simply invert it and recurse
    if (!keep_inside) {
        SimpleMesh clip_mesh_inv(clip_mesh.triangles, clip_mesh.positions);
        flip_orientation(clip_mesh_inv);
        return mesh::clip_on_mesh(mesh, clip_mesh_inv, !keep_inside);
    }
    
    // Convert both meshes to cgal and use their clipping logic.
    using UvMap = cgal::Mesh::Property_map<cgal::VertexIndex, glm::dvec2>;

    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(mesh);
    cgal::Mesh cgal_clip_mesh = convert::to_cgal_mesh(clip_mesh);
    cgal::validate(cgal_mesh);
    cgal::validate(cgal_clip_mesh);
    DEBUG_ASSERT(CGAL::Polygon_mesh_processing::does_bound_a_volume(cgal_clip_mesh));

    bool success;
    bool has_intersections;
    if (mesh.has_uvs()) {
        UvMap uv_map = cgal_mesh.property_map<cgal::VertexIndex, glm::dvec2>("v:uv").value();
        UvInterpolatorVisitor<cgal::Mesh, UvMap> visitor(uv_map, cgal_mesh);
        const auto params = CGAL::Polygon_mesh_processing::parameters::visitor(visitor);
        success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh, params);
        has_intersections = !visitor.intersections.empty();
    } else {
        HasIntersectionsVisitor<cgal::Mesh> visitor;
        const auto params = CGAL::Polygon_mesh_processing::parameters::visitor(visitor);
        success = CGAL::Polygon_mesh_processing::clip(cgal_mesh, cgal_clip_mesh, params);
        has_intersections = visitor.has_intersections;
    }
    if (!success) {
        throw std::runtime_error("CGAL::Polygon_mesh_processing::clip failed");
    }
    if (cgal_mesh.number_of_faces() == 0) {
        return Cow<SimpleMesh>::owned(SimpleMesh());
    }
    if (!has_intersections && cgal_mesh.number_of_faces() == mesh.face_count()) {
        return Cow<SimpleMesh>::borrowed(mesh);
    }
    
    cgal_mesh.collect_garbage();
    cgal::validate(cgal_mesh);
    SimpleMesh result = convert::to_simple_mesh(cgal_mesh);
    if (mesh.has_texture()) {
        result.texture = mesh.texture.value();
    }
    return Cow<SimpleMesh>::owned(std::move(result));
}

}
