module;

#include <print>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

export module mesh;

export import vecmath;

export struct Triangle {
    std::array<Vec3, 3> positions;
    Vec3 normal;
    Vec3 color;
    Vec3 emission;

    Vec3 centroid() const
    {
        return (1.0f / 3.0f) * (positions[0] + positions[1] + positions[2]);
    }
};

export std::vector<Triangle> loadMesh(const std::string &path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());
    if (!warn.empty()) {
        std::println(stderr, "TinyObj warn: {}", warn);
    }
    if (!err.empty()) {
        std::println(stderr, "TinyObj error: {}", warn);
    }
    if (!ret) {
        return { };
    }

    std::vector<Triangle> triangles;

    bool first = true;
    for (const auto &shape : shapes) {
        const auto &mesh = shape.mesh;
        std::size_t indexOffset = 0;
        const auto faceCount = mesh.num_face_vertices.size();
        for (std::size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
            const auto materialIndex = mesh.material_ids[faceIndex];
            assert(materialIndex >= 0);
            const auto& material = materials[materialIndex];

            auto vertexAt = [&](std::size_t i) -> Vec3 {
                const tinyobj::index_t index = mesh.indices[indexOffset + i];
                const auto x = attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 0];
                const auto y = attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 1];
                const auto z = attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 2];
                return { x, y, z };
            };

            const auto normal = normalize(crossProduct(vertexAt(1) - vertexAt(0), vertexAt(2) - vertexAt(0)));
            const auto color = Vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
            const auto emission = Vec3(material.emission[0], material.emission[1], material.emission[2]);

            const std::size_t vertCount = mesh.num_face_vertices[faceIndex];
            for (std::size_t i = 1; i < vertCount - 1; ++i) {
                Triangle triangle;
                triangle.positions[0] = vertexAt(0);
                triangle.positions[1] = vertexAt(i);
                triangle.positions[2] = vertexAt(i + 1);
                triangle.color = color;
                triangle.emission = emission;
                triangle.normal = normal;
                triangles.push_back(triangle);
            }

            indexOffset += vertCount;
        }
        first = false;
    }

    return triangles;
}
