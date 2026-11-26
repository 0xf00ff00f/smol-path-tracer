import gpu;

#ifdef USE_RENDERDOC
import renderdoc;
#endif

#include <algorithm>
#include <cassert>
#include <print>
#include <cstdio>
#include <memory>
#include <numeric>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

struct alignas(16) Vec3 {
    float x, y, z;

    bool operator==(const Vec3& other) const = default;

    float operator[](std::size_t i) const
    {
        switch (i) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
        default:
            return z;
        }
    };
};
static_assert(sizeof(Vec3) == 4 * sizeof(float));

Vec3 crossProduct(const Vec3& a, const Vec3& b)
{
  return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

Vec3 operator+(const Vec3 &a, const Vec3 &b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 operator*(float s, const Vec3 &v)
{
    return { v.x * s, v.y * s, v.z * s };
}

Vec3 operator*(const Vec3 &v, float s)
{
    return s * v;
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

Vec3 min(const Vec3 &a, const Vec3 &b)
{
    return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

Vec3 max(const Vec3 &a, const Vec3 &b)
{
    return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

struct Triangle {
    std::array<Vec3, 3> positions;
    Vec3 normal;
    Vec3 color;
    Vec3 emission;

    Vec3 centroid() const
    {
        return (1.0f / 3.0f) * (positions[0] + positions[1] + positions[2]);
    }
};

struct BVHNode {
    static constexpr auto kMaxLeafTriangles = 8;

    uint32_t triangleCount; //  0 if internal node
    std::array<int32_t, kMaxLeafTriangles> triangles;
    uint32_t leftIndex;
    uint32_t rightIndex;
    Vec3 boxMin;
    Vec3 boxMax;
};
static_assert(offsetof(BVHNode, triangles) == 4);
static_assert(offsetof(BVHNode, leftIndex) == 36);
static_assert(offsetof(BVHNode, rightIndex) == 40);
static_assert(offsetof(BVHNode, boxMin) == 48);
static_assert(offsetof(BVHNode, boxMax) == 64);
static_assert(sizeof(BVHNode) == 80);

class BVHTreeBuilder
{
public:
    explicit BVHTreeBuilder(std::span<const Triangle> triangles)
        : m_triangles{ triangles }
    {
    }

    std::vector<BVHNode> build();

private:
    std::size_t buildImpl(std::span<std::size_t> indices);

    std::span<const Triangle> m_triangles;
    std::vector<BVHNode> m_tree;
};

std::vector<BVHNode>
BVHTreeBuilder::build()
{
    std::vector<std::size_t> indices(m_triangles.size());
    std::ranges::iota(indices, 0);
    m_tree.clear();
    buildImpl(indices);

    const auto isSane = [this] {
        std::vector<std::size_t> stack;
        std::size_t visited = 0;
        std::size_t triangles = 0;
        std::vector<bool> visitedTriangles(m_triangles.size());
        stack.push_back(0);
        while (!stack.empty()) {
            std::size_t curIndex = stack.back();
            stack.pop_back();
            ++visited;
            assert(curIndex < m_tree.size());
            const auto &node = m_tree[curIndex];
            if (node.triangleCount > 0) {
                // leaf
                triangles += node.triangleCount;
                for (auto triangleIndex : node.triangles) {
                    if (triangleIndex != -1) {
                        assert(triangleIndex >= 0 && triangleIndex < m_triangles.size());
                        assert(!visitedTriangles[triangleIndex]);
                        visitedTriangles[triangleIndex] = true;
                    }
                }
            } else {
                assert(node.leftIndex > 0);
                stack.push_back(node.leftIndex);
                assert(node.rightIndex > 0);
                stack.push_back(node.rightIndex);
            }
        }
        return visited == m_tree.size() && triangles == m_triangles.size() && std::ranges::all_of(visitedTriangles, [](bool b) { return b; });
    };
    assert(isSane());

    return m_tree;
}

std::size_t
BVHTreeBuilder::buildImpl(std::span<std::size_t> indices)
{
    assert(!indices.empty());

    auto nodeIndex = m_tree.size();
    m_tree.resize(m_tree.size() + 1);

    BVHNode node;

    auto &boxMin = node.boxMin;
    boxMin = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

    auto &boxMax = node.boxMax;
    boxMax = Vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

    for (const std::size_t index : indices) {
        const auto &triangle = m_triangles[index];
        for (const auto &v : triangle.positions) {
            boxMin = min(boxMin, v);
            boxMax = max(boxMax, v);
        }
    }

    node.triangleCount = 0;
    node.triangles.fill(-1);

    if (indices.size() <= BVHNode::kMaxLeafTriangles) {
        node.triangleCount = indices.size();
        std::ranges::copy(indices, node.triangles.begin());
        node.leftIndex = node.rightIndex = 0;
        std::println("BVH tree leaf: {} triangles, box: {},{},{} -> {},{},{}", indices.size(), boxMin.x, boxMin.y, boxMin.z, boxMax.x, boxMax.y, boxMax.z);
    } else {
        Vec3 extent = boxMax - boxMin;

        std::size_t splitAxis = 0;
        if (extent[1] > extent[splitAxis])
            splitAxis = 1;
        if (extent[2] > extent[splitAxis])
            splitAxis = 2;

        // sort centroids along longest axis, split at median index
        std::ranges::sort(indices, [this, splitAxis](std::size_t a, std::size_t b) {
            const auto centroidA = m_triangles[a].centroid();
            const auto centroidB = m_triangles[b].centroid();
            return centroidA[splitAxis] < centroidB[splitAxis];
        });

        auto splitIt = std::next(indices.begin(), indices.size() / 2);
        node.leftIndex = buildImpl({ indices.begin(), splitIt });
        node.rightIndex = buildImpl({ splitIt, indices.end() });
    }

    m_tree[nodeIndex] = node;

    return nodeIndex;
}

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

#ifdef USE_RENDERDOC
    renderdoc::FrameCapture capture(physDevice.name().c_str());
#endif

    const auto triangles = loadMesh("cube-monkey.obj");

    gpu::TypedBuffer<Triangle> meshBuffer = device.createTypedBuffer<Triangle>(triangles.size());
    std::ranges::copy(triangles, meshBuffer.map().begin());
    meshBuffer.unmap();

    auto bvhTree = BVHTreeBuilder{ triangles }.build();
    std::println("{} nodes", bvhTree.size());

    gpu::TypedBuffer<BVHNode> bvhTreeBuffer = device.createTypedBuffer<BVHNode>(bvhTree.size());
    std::ranges::copy(bvhTree, bvhTreeBuffer.map().begin());
    bvhTreeBuffer.unmap();

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
    pipeline0.bind(1, bvhTreeBuffer);
    pipeline0.bind(2, imageBuffer0);
    pipeline0.bind(3, imageBuffer1);
    pipeline0.pushConstants(constants);

    // image 1 -> 0
    gpu::Pipeline pipeline1 = program.createPipeline();
    pipeline1.bind(0, meshBuffer);
    pipeline1.bind(1, bvhTreeBuffer);
    pipeline1.bind(2, imageBuffer1);
    pipeline1.bind(3, imageBuffer0);
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
