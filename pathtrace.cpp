import gpu;
import mesh;

#ifdef USE_RENDERDOC
import renderdoc;
#endif

#include <algorithm>
#include <cassert>
#include <print>
#include <cstdio>
#include <memory>
#include <numeric>
#include <unistd.h>

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

struct Color {
    float r, g, b, a;
};

struct Camera {
    Vec3 position;
    Vec3 target;
    float roll;
    float focalLength;
};

class PathTracer
{
public:
    PathTracer();

    void setResolution(std::size_t width, std::size_t height);
    std::size_t width() const { return m_width; }
    std::size_t height() const { return m_height; }

    void setCamera(const Camera &camera);
    Camera camera() const { return m_camera; }

    void setMesh(std::span<const Triangle> mesh);

    std::vector<Color> render(std::size_t passes);

    PathTracer(const PathTracer &) = delete;
    PathTracer &operator=(const PathTracer &) = delete;

    PathTracer(PathTracer &&) = delete;
    PathTracer &operator=(PathTracer &&) = delete;

private:
    bool createResources();

    struct PushConstants {
        uint32_t triangleCount;
        uint32_t frameIndex;
        std::array<uint32_t, 2> resolution;
        Camera camera;
    };

    gpu::Instance m_instance;
    gpu::Device m_device;
    gpu::Program m_program;
    gpu::TypedBuffer<Triangle> m_meshBuffer;
    gpu::TypedBuffer<BVHNode> m_bvhTreeBuffer;
    gpu::TypedBuffer<Color> m_imageBuffer0;
    gpu::TypedBuffer<Color> m_imageBuffer1;
    gpu::Pipeline m_pipeline0;
    gpu::Pipeline m_pipeline1;
    PushConstants m_pushConstants;

    std::size_t m_width{ 640 };
    std::size_t m_height{ 400 };
    Camera m_camera;
    std::vector<Triangle> m_mesh;
    std::vector<BVHNode> m_bvhTree;
    bool m_dirty{ true };
};

PathTracer::PathTracer()
{
    const std::vector<gpu::PhysDevice> physDevices = m_instance.physDevices();
    if (physDevices.empty()) {
        std::println(stderr, "No Vulkan devices");
        std::abort();
    }

    const gpu::PhysDevice &physDevice = physDevices.front();
    std::println("Device: {} ({})", physDevice.name(), physDevice.type());

    m_device = physDevice.createDevice();

    m_program = m_device.createProgram("pathtrace.comp.spv");

    m_camera.position = Vec3(4.5, 1.5, 4.5);
    m_camera.target = Vec3(0.0, 0.0, 0.0);
    m_camera.roll = 0.0;
    m_camera.focalLength = 1.0;
}

void PathTracer::setResolution(std::size_t width, std::size_t height)
{
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;
    m_dirty = true;
}

void PathTracer::setMesh(std::span<const Triangle> mesh)
{
    m_mesh.assign(mesh.begin(), mesh.end());
    m_bvhTree = BVHTreeBuilder{ m_mesh }.build();
    m_dirty = true;
}

void PathTracer::setCamera(const Camera &camera)
{
    m_camera = camera;
}

bool PathTracer::createResources()
{
    if (m_mesh.empty()) {
        std::println(stderr, "Mesh not set");
        return false;
    }

    m_imageBuffer0 = m_device.createTypedBuffer<Color>(m_width * m_height);
    m_imageBuffer1 = m_device.createTypedBuffer<Color>(m_width * m_height);

    m_meshBuffer = m_device.createTypedBuffer<Triangle>(m_mesh.size());
    std::ranges::copy(m_mesh, m_meshBuffer.map().begin());
    m_meshBuffer.unmap();

    m_bvhTreeBuffer = m_device.createTypedBuffer<BVHNode>(m_bvhTree.size());
    std::ranges::copy(m_bvhTree, m_bvhTreeBuffer.map().begin());
    m_bvhTreeBuffer.unmap();

    // image 0 -> 1
    m_pipeline0 = m_program.createPipeline();
    m_pipeline0.bind(0, m_meshBuffer);
    m_pipeline0.bind(1, m_bvhTreeBuffer);
    m_pipeline0.bind(2, m_imageBuffer0);
    m_pipeline0.bind(3, m_imageBuffer1);
    m_pipeline0.pushConstants(m_pushConstants);

    // image 1 -> 0
    m_pipeline1 = m_program.createPipeline();
    m_pipeline1.bind(0, m_meshBuffer);
    m_pipeline1.bind(1, m_bvhTreeBuffer);
    m_pipeline1.bind(2, m_imageBuffer1);
    m_pipeline1.bind(3, m_imageBuffer0);
    m_pipeline1.pushConstants(m_pushConstants);

    return true;
}

std::vector<Color> PathTracer::render(std::size_t passes)
{
    if (m_dirty) {
        if (!createResources())
            return { };
        m_dirty = false;
    }

    m_pushConstants.triangleCount = m_mesh.size();
    m_pushConstants.resolution = { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height) };
    m_pushConstants.camera = m_camera;

    std::ranges::fill(m_imageBuffer0.map(), Color{ 0.0f, 0.0f, 0.0f, 0.0f });
    m_imageBuffer0.unmap();

#ifdef USE_RENDERDOC
    renderdoc::FrameCapture capture("pathtrace");
#endif

    constexpr auto kWorkgroupSize = 32;

    for (std::size_t i = 0; i < passes; ++i) {
        std::println("rendering pass {}", i);

        m_pushConstants.frameIndex = i;

        if ((i % 2) == 0) {
            // image 0 -> 1
            m_pipeline0.dispatch((m_width + kWorkgroupSize - 1) / kWorkgroupSize, (m_height + kWorkgroupSize - 1) / kWorkgroupSize);
        } else {
            // image 1 -> 0
            m_pipeline1.dispatch((m_width + kWorkgroupSize - 1) / kWorkgroupSize, (m_height + kWorkgroupSize - 1) / kWorkgroupSize);
        }
    }

    const auto &finalBuffer = ((passes % 2) == 1) ? m_imageBuffer1 : m_imageBuffer0;
    const auto pixels = finalBuffer.map();
    std::vector<Color> image{ pixels.begin(), pixels.end() };
    finalBuffer.unmap();
    return image;
}

int main(int argc, char *argv[])
{
    std::size_t passes = 20;
    std::size_t width = 640;
    std::size_t height = 400;
    std::string outfile{ "output.ppm" };

    const auto usage = [argv] { std::println(stderr, "Usage: {} [-w width] [-h height] [-p passes] [-o outfile] mesh", argv[0]); };

    int opt;
    while ((opt = getopt(argc, argv, "w:h:p:o:")) != -1) {
        switch (opt) {
        case 'w':
            width = std::stoi(optarg);
            break;
        case 'h':
            height = std::stoi(optarg);
            break;
        case 'p':
            passes = std::stoi(optarg);
            break;
        case 'o':
            outfile = optarg;
            break;
        default:
            usage();
            return 1;
        }
    }

    if (optind >= argc) {
        usage();
        return 1;
    }

    const auto mesh = argv[optind];

    PathTracer pathTracer;
    pathTracer.setMesh(loadMesh(mesh));
    pathTracer.setResolution(width, height);

    const auto pixels = pathTracer.render(passes);

    std::unique_ptr<FILE, int (*)(FILE *)> file{ fopen(outfile.c_str(), "wb"), &fclose };
    std::println(file.get(), "P6");
    std::println(file.get(), "{} {}", pathTracer.width(), pathTracer.height());
    std::println(file.get(), "255");
    for (const auto pixel : pixels) {
        constexpr auto kGamma = 1.0f / 2.2f;
        std::fputc(static_cast<int>(std::min(std::powf(pixel.r, kGamma), 1.0f) * 255.0f), file.get());
        std::fputc(static_cast<int>(std::min(std::powf(pixel.g, kGamma), 1.0f) * 255.0f), file.get());
        std::fputc(static_cast<int>(std::min(std::powf(pixel.b, kGamma), 1.0f) * 255.0f), file.get());
    }
}
