import gpu;

#include <cassert>
#include <print>
#include <cstdio>
#include <memory>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

struct alignas(16) Vec3 {
    float x, y, z;
    bool operator==(const Vec3& other) const = default;
};

Vec3 crossProduct(const Vec3& a, const Vec3& b)
{
  return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(const Vec3& v)
{
    return std::sqrt(dot(v, v));
}

Vec3 normalize(const Vec3& v)
{
    const auto l = length(v);
    return { v.x / l, v.y / l, v.z / l };
}

struct Triangle {
    std::array<Vec3, 3> positions;
    Vec3 normal;
    Vec3 color;
    Vec3 emission;
};

std::vector<Triangle> loadMesh(const std::string &path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());
    if (!warn.empty()) {
        std::println("TinyObj warn: {}", warn);
    }
    if (!err.empty()) {
        std::println("TinyObj error: {}", warn);
    }
    if (!ret) {
        return { };
    }

    std::vector<Triangle> triangles;

    bool first = true;
    for (const auto& shape : shapes) {
        const auto& mesh = shape.mesh;
        std::size_t indexOffset = 0;
        const auto faceCount = mesh.num_face_vertices.size();
        for (std::size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
            tinyobj::material_t material;
            const auto materialIndex = mesh.material_ids[faceIndex];
            assert(materialIndex >= 0);
            material = materials[materialIndex];

            auto vertexAt = [&, indexOffset](std::size_t i) -> Vec3 {
                const tinyobj::index_t index = mesh.indices[indexOffset + i];
                const auto x = attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 0];
                const auto y = attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 1];
                const auto z = attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 2];
                return { x, y, z };
            };

            auto normal = normalize(crossProduct(vertexAt(1) - vertexAt(0), vertexAt(2) - vertexAt(0)));
            const auto color = Vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
            const auto emission = Vec3(material.emission[0], material.emission[1], material.emission[2]);
            if (first) {
                // flip normals for first object (the cube)
                normal.x = -normal.x;
                normal.y = -normal.y;
                normal.z = -normal.z;
            }

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

int main()
{
    gpu::Instance instance;

    const std::vector<gpu::PhysDevice> physDevices = instance.physDevices();
    if (physDevices.empty())
        return 1;
    const gpu::PhysDevice &physDevice = physDevices.front();
    std::println("Device: {} ({})", physDevice.name(), physDevice.type());
    gpu::Device device = physDevice.createDevice();

    const auto triangles = loadMesh("cube-monkey.obj");
    gpu::TypedBuffer<Triangle> meshBuffer = device.createTypedBuffer<Triangle>(triangles.size());
    std::ranges::copy(triangles, meshBuffer.map().begin());
    meshBuffer.unmap();

    constexpr auto kWidth = 600;
    constexpr auto kHeight = 400;

    struct Constants {
        uint32_t triangleCount;
        uint32_t frameIndex;
        std::array<uint32_t, 2> resolution;
    } constants = {
        .triangleCount = static_cast<uint32_t>(triangles.size()),
        .frameIndex = 0,
        .resolution = { kWidth, kHeight }
    };

    struct Color {
        float r, g, b, a;
    };
    gpu::TypedBuffer<Color> imageBuffer0 = device.createTypedBuffer<Color>(kWidth * kHeight);
    std::ranges::fill(imageBuffer0.map(), Color{ 0.0f, 0.0f, 0.0f, 0.0f });
    imageBuffer0.unmap();

    gpu::TypedBuffer<Color> imageBuffer1 = device.createTypedBuffer<Color>(kWidth * kHeight);

    gpu::Program program = device.createProgram("pathtrace.comp.spv");

    // image 0 -> 1
    gpu::Pipeline pipeline0 = program.createPipeline();
    pipeline0.bind(0, meshBuffer);
    pipeline0.bind(1, imageBuffer0);
    pipeline0.bind(2, imageBuffer1);
    pipeline0.pushConstants(constants);

    // image 1 -> 0
    gpu::Pipeline pipeline1 = program.createPipeline();
    pipeline1.bind(0, meshBuffer);
    pipeline1.bind(1, imageBuffer1);
    pipeline1.bind(2, imageBuffer0);
    pipeline1.pushConstants(constants);

    constexpr auto kWorkgroupSize = 32;
    constexpr auto kFrames = 20;

    for (std::size_t i = 0; i < kFrames; ++i) {
        std::println("rendering frame {}", i);

        constants.frameIndex = i;

        if ((i % 2) == 0) {
            // image 0 -> 1
            pipeline0.dispatch((kWidth + kWorkgroupSize - 1) / kWorkgroupSize, (kHeight + kWorkgroupSize - 1) / kWorkgroupSize);
        } else {
            // image 1 -> 2
            pipeline1.dispatch((kWidth + kWorkgroupSize - 1) / kWorkgroupSize, (kHeight + kWorkgroupSize - 1) / kWorkgroupSize);
        }
    }

    const auto& finalBuffer = ((kFrames % 2) == 1) ? imageBuffer1 : imageBuffer0;
    const auto pixels = finalBuffer.map();

    std::unique_ptr<FILE, int (*)(FILE *)> file{ fopen("output.ppm", "wb"), &fclose };
    std::println(file.get(), "P6");
    std::println(file.get(), "{} {}", kWidth, kHeight);
    std::println(file.get(), "255");
    for (const auto pixel : pixels) {
        constexpr auto kGamma = 1.0f / 2.2f;
        std::fputc(static_cast<int>(std::min(std::powf(pixel.r, kGamma), 1.0f) * 255.0f), file.get());
        std::fputc(static_cast<int>(std::min(std::powf(pixel.g, kGamma), 1.0f) * 255.0f), file.get());
        std::fputc(static_cast<int>(std::min(std::powf(pixel.b, kGamma), 1.0f) * 255.0f), file.get());
    }
}
