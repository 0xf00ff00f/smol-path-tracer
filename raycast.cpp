import gpu;

#include <cassert>
#include <print>
#include <cstdio>
#include <memory>

struct alignas(16) Vec3 {
    float x, y, z;
};

struct Triangle {
    std::array<Vec3, 3> positions;
    Vec3 normal;
    Vec3 color;
    Vec3 emission;
};

constexpr auto kTriangleCount = 30;

std::span<const Triangle> meshTriangles()
{
    static const auto tris = [] {
        std::array<Triangle, kTriangleCount> tris;
        // copied from https://www.intel.com/content/www/us/en/developer/videos/path-tracing-workshop-part-1.html
        tris[0].positions[0] = Vec3(0.000000133, -0.559199989, 0.548799932);
        tris[0].positions[1] = Vec3(0.555999935, -0.559199989, 0.000000040);
        tris[0].positions[2] = Vec3(0.000000133, -0.559199989, 0.000000040);
        tris[1].positions[0] = Vec3(0.313999921, -0.455999970, 0.329999954);
        tris[1].positions[1] = Vec3(0.313999921, -0.455999970, 0.000000040);
        tris[1].positions[2] = Vec3(0.472000152, -0.406000137, 0.000000040);
        tris[2].positions[0] = Vec3(0.000000133, -0.559199989, 0.000000040);
        tris[2].positions[1] = Vec3(0.555999935, -0.559199989, 0.000000040);
        tris[2].positions[2] = Vec3(0.555999935, -0.000000119, 0.000000040);
        tris[3].positions[0] = Vec3(0.264999926, -0.296000093, 0.329999954);
        tris[3].positions[1] = Vec3(0.264999926, -0.296000093, 0.000000040);
        tris[3].positions[2] = Vec3(0.313999921, -0.455999970, 0.000000040);
        tris[4].positions[0] = Vec3(0.423000127, -0.246999890, 0.000000040);
        tris[4].positions[1] = Vec3(0.472000152, -0.406000137, 0.329999954);
        tris[4].positions[2] = Vec3(0.472000152, -0.406000137, 0.000000040);
        tris[5].positions[0] = Vec3(0.264999926, -0.296000093, 0.329999954);
        tris[5].positions[1] = Vec3(0.313999921, -0.455999970, 0.000000040);
        tris[5].positions[2] = Vec3(0.313999921, -0.455999970, 0.329999954);
        tris[6].positions[0] = Vec3(0.313999921, -0.455999970, 0.329999954);
        tris[6].positions[1] = Vec3(0.472000152, -0.406000137, 0.000000040);
        tris[6].positions[2] = Vec3(0.472000152, -0.406000137, 0.329999954);
        tris[7].positions[0] = Vec3(0.240000039, -0.271999955, 0.165000007);
        tris[7].positions[1] = Vec3(0.082000092, -0.225000143, 0.165000007);
        tris[7].positions[2] = Vec3(0.082000092, -0.225000143, 0.000000040);
        tris[8].positions[0] = Vec3(0.240000039, -0.271999955, 0.165000007);
        tris[8].positions[1] = Vec3(0.082000092, -0.225000143, 0.000000040);
        tris[8].positions[2] = Vec3(0.240000039, -0.271999955, 0.000000040);
        tris[9].positions[0] = Vec3(0.290000081, -0.113999903, 0.000000040);
        tris[9].positions[1] = Vec3(0.240000039, -0.271999955, 0.165000007);
        tris[9].positions[2] = Vec3(0.240000039, -0.271999955, 0.000000040);
        tris[10].positions[0] = Vec3(0.082000092, -0.225000143, 0.000000040);
        tris[10].positions[1] = Vec3(0.130000070, -0.064999968, 0.165000007);
        tris[10].positions[2] = Vec3(0.130000070, -0.064999968, 0.000000040);
        tris[11].positions[0] = Vec3(0.082000092, -0.225000143, 0.000000040);
        tris[11].positions[1] = Vec3(0.082000092, -0.225000143, 0.165000007);
        tris[11].positions[2] = Vec3(0.130000070, -0.064999968, 0.165000007);
        tris[12].positions[0] = Vec3(0.000000133, -0.559199989, 0.000000040);
        tris[12].positions[1] = Vec3(0.555999935, -0.000000119, 0.000000040);
        tris[12].positions[2] = Vec3(0.000000133, -0.000000119, 0.000000040);
        tris[13].positions[0] = Vec3(0.130000070, -0.064999968, 0.000000040);
        tris[13].positions[1] = Vec3(0.290000081, -0.114000171, 0.165000007);
        tris[13].positions[2] = Vec3(0.290000081, -0.113999903, 0.000000040);
        tris[14].positions[0] = Vec3(0.290000081, -0.113999903, 0.000000040);
        tris[14].positions[1] = Vec3(0.290000081, -0.114000171, 0.165000007);
        tris[14].positions[2] = Vec3(0.240000039, -0.271999955, 0.165000007);
        tris[15].positions[0] = Vec3(0.130000070, -0.064999968, 0.000000040);
        tris[15].positions[1] = Vec3(0.130000070, -0.064999968, 0.165000007);
        tris[15].positions[2] = Vec3(0.290000081, -0.114000171, 0.165000007);
        tris[16].positions[0] = Vec3(0.000000133, -0.559199989, 0.000000040);
        tris[16].positions[1] = Vec3(0.000000133, -0.000000119, 0.000000040);
        tris[16].positions[2] = Vec3(0.000000133, -0.000000119, 0.548799932);
        tris[17].positions[0] = Vec3(0.130000070, -0.064999968, 0.165000007);
        tris[17].positions[1] = Vec3(0.082000092, -0.225000143, 0.165000007);
        tris[17].positions[2] = Vec3(0.240000039, -0.271999955, 0.165000007);
        tris[18].positions[0] = Vec3(0.130000070, -0.064999968, 0.165000007);
        tris[18].positions[1] = Vec3(0.240000039, -0.271999955, 0.165000007);
        tris[18].positions[2] = Vec3(0.290000081, -0.114000171, 0.165000007);
        tris[19].positions[0] = Vec3(0.423000127, -0.247000158, 0.329999954);
        tris[19].positions[1] = Vec3(0.423000127, -0.246999890, 0.000000040);
        tris[19].positions[2] = Vec3(0.264999926, -0.296000093, 0.000000040);
        tris[20].positions[0] = Vec3(0.423000127, -0.247000158, 0.329999954);
        tris[20].positions[1] = Vec3(0.264999926, -0.296000093, 0.000000040);
        tris[20].positions[2] = Vec3(0.264999926, -0.296000093, 0.329999954);
        tris[21].positions[0] = Vec3(0.423000127, -0.246999890, 0.000000040);
        tris[21].positions[1] = Vec3(0.423000127, -0.247000158, 0.329999954);
        tris[21].positions[2] = Vec3(0.472000152, -0.406000137, 0.329999954);
        tris[22].positions[0] = Vec3(0.555999935, -0.000000119, 0.548799932);
        tris[22].positions[1] = Vec3(0.555999935, -0.000000119, 0.000000040);
        tris[22].positions[2] = Vec3(0.555999935, -0.559199989, 0.000000040);
        tris[23].positions[0] = Vec3(0.000000133, -0.559199989, 0.000000040);
        tris[23].positions[1] = Vec3(0.000000133, -0.000000119, 0.548799932);
        tris[23].positions[2] = Vec3(0.000000133, -0.559199989, 0.548799932);
        tris[24].positions[0] = Vec3(0.000000133, -0.000000119, 0.548799932);
        tris[24].positions[1] = Vec3(0.555999935, -0.559199989, 0.548799932);
        tris[24].positions[2] = Vec3(0.000000133, -0.559199989, 0.548799932);
        tris[25].positions[0] = Vec3(0.000000133, -0.559199989, 0.548799932);
        tris[25].positions[1] = Vec3(0.555999935, -0.559199989, 0.548799932);
        tris[25].positions[2] = Vec3(0.555999935, -0.559199989, 0.000000040);
        tris[26].positions[0] = Vec3(0.472000152, -0.406000137, 0.329999954);
        tris[26].positions[1] = Vec3(0.264999926, -0.296000093, 0.329999954);
        tris[26].positions[2] = Vec3(0.313999921, -0.455999970, 0.329999954);
        tris[27].positions[0] = Vec3(0.555999935, -0.000000119, 0.548799932);
        tris[27].positions[1] = Vec3(0.555999935, -0.559199989, 0.000000040);
        tris[27].positions[2] = Vec3(0.555999935, -0.559199989, 0.548799932);
        tris[28].positions[0] = Vec3(0.472000152, -0.406000137, 0.329999954);
        tris[28].positions[1] = Vec3(0.423000127, -0.247000158, 0.329999954);
        tris[28].positions[2] = Vec3(0.264999926, -0.296000093, 0.329999954);
        tris[29].positions[0] = Vec3(0.000000133, -0.000000119, 0.548799932);
        tris[29].positions[1] = Vec3(0.555999935, -0.000000119, 0.548799932);
        tris[29].positions[2] = Vec3(0.555999935, -0.559199989, 0.548799932);
        tris[0].normal = Vec3(0.0, 1.0, 0.0);
        tris[1].normal = Vec3(0.301707575, -0.953400513, 0.0);
        tris[2].normal = Vec3(0.0, 0.0, 1.0);
        tris[3].normal = Vec3(-0.956165759, -0.292825958, -0.0);
        tris[4].normal = Vec3(0.955649049, 0.294507888, 0.0);
        tris[5].normal = Vec3(-0.956165759, -0.292825958, 0.0);
        tris[6].normal = Vec3(0.301707575, -0.953400513, 0.0);
        tris[7].normal = Vec3(-0.285119946, -0.958491845, 0.0);
        tris[8].normal = Vec3(-0.285119946, -0.958491845, -0.0);
        tris[9].normal = Vec3(0.953400053, -0.301709030, 0.0);
        tris[10].normal = Vec3(-0.957826408, 0.287347476, 0.0);
        tris[11].normal = Vec3(-0.957826408, 0.287347476, 0.0);
        tris[12].normal = Vec3(0.0, 0.0, 1.0);
        tris[13].normal = Vec3(0.292825408, 0.956165927, 0.000001554);
        tris[14].normal = Vec3(0.953399906, -0.301709496, -0.000000490);
        tris[15].normal = Vec3(0.292826874, 0.956165478, -0.0);
        tris[16].normal = Vec3(1.0, 0.0, 0.0);
        tris[17].normal = Vec3(0.0, 0.0, 1.0);
        tris[18].normal = Vec3(0.0, 0.0, 1.0);
        tris[19].normal = Vec3(-0.296209850, 0.955122885, 0.000000776);
        tris[20].normal = Vec3(-0.296208371, 0.955123343, 0.0);
        tris[21].normal = Vec3(0.955648909, 0.294508341, 0.000000239);
        tris[22].normal = Vec3(-1.0, 0.0, -0.0);
        tris[23].normal = Vec3(1.0, 0.0, 0.0);
        tris[24].normal = Vec3(0.0, 0.0, -1.0);
        tris[25].normal = Vec3(-0.0, 1.0, 0.0);
        tris[26].normal = Vec3(0.0, 0.0, 1.0);
        tris[27].normal = Vec3(-1.0, -0.0, 0.0);
        tris[28].normal = Vec3(0.0, 0.0, 1.0);
        tris[29].normal = Vec3(0.0, 0.0, -1.0);
        tris[0].color = Vec3(0.874000013, 0.874000013, 0.875000000);
        tris[1].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[2].color = Vec3(0.874000013, 0.874000013, 0.875000000);
        tris[3].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[4].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[5].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[6].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[7].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[8].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[9].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[10].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[11].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[12].color = Vec3(0.874000013, 0.874000013, 0.875000000);
        tris[13].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[14].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[15].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[16].color = Vec3(0.289999992, 0.663999975, 0.324999988);
        tris[17].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[18].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[19].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[20].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[21].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[22].color = Vec3(0.730000019, 0.246000007, 0.250999987);
        tris[23].color = Vec3(0.289999992, 0.663999975, 0.324999988);
        tris[24].color = Vec3(0.0, 0.0, 0.0);
        tris[25].color = Vec3(0.874000013, 0.874000013, 0.875000000);
        tris[26].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[27].color = Vec3(0.730000019, 0.246000007, 0.250999987);
        tris[28].color = Vec3(0.839039981, 0.839039981, 0.839999974);
        tris[29].color = Vec3(0.0, 0.0, 0.0);
        tris[0].emission = Vec3(0.0, 0.0, 0.0);
        tris[1].emission = Vec3(0.0, 0.0, 0.0);
        tris[2].emission = Vec3(0.0, 0.0, 0.0);
        tris[3].emission = Vec3(0.0, 0.0, 0.0);
        tris[4].emission = Vec3(0.0, 0.0, 0.0);
        tris[5].emission = Vec3(0.0, 0.0, 0.0);
        tris[6].emission = Vec3(0.0, 0.0, 0.0);
        tris[7].emission = Vec3(0.0, 0.0, 0.0);
        tris[8].emission = Vec3(0.0, 0.0, 0.0);
        tris[9].emission = Vec3(0.0, 0.0, 0.0);
        tris[10].emission = Vec3(0.0, 0.0, 0.0);
        tris[11].emission = Vec3(0.0, 0.0, 0.0);
        tris[12].emission = Vec3(0.0, 0.0, 0.0);
        tris[13].emission = Vec3(0.0, 0.0, 0.0);
        tris[14].emission = Vec3(0.0, 0.0, 0.0);
        tris[15].emission = Vec3(0.0, 0.0, 0.0);
        tris[16].emission = Vec3(0.0, 0.0, 0.0);
        tris[17].emission = Vec3(0.0, 0.0, 0.0);
        tris[18].emission = Vec3(0.0, 0.0, 0.0);
        tris[19].emission = Vec3(0.0, 0.0, 0.0);
        tris[20].emission = Vec3(0.0, 0.0, 0.0);
        tris[21].emission = Vec3(0.0, 0.0, 0.0);
        tris[22].emission = Vec3(0.0, 0.0, 0.0);
        tris[23].emission = Vec3(0.0, 0.0, 0.0);
        tris[24].emission = Vec3(3.0, 3.0, 3.0);
        tris[25].emission = Vec3(0.0, 0.0, 0.0);
        tris[26].emission = Vec3(0.0, 0.0, 0.0);
        tris[27].emission = Vec3(0.0, 0.0, 0.0);
        tris[28].emission = Vec3(0.0, 0.0, 0.0);
        tris[29].emission = Vec3(3.0, 3.0, 3.0);
        return tris;
    }();
    return tris;
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

    gpu::TypedBuffer<Triangle> meshBuffer = device.createTypedBuffer<Triangle>(kTriangleCount);
    std::ranges::copy(meshTriangles(), meshBuffer.map().begin());
    meshBuffer.unmap();

    constexpr auto kWidth = 600;
    constexpr auto kHeight = 400;
    struct Color {
        float r, g, b, a;
    };
    gpu::TypedBuffer<Color> imageBuffer = device.createTypedBuffer<Color>(kWidth * kHeight);

    gpu::Program program = device.createProgram("raycast.comp.spv");
    gpu::Pipeline pipeline = program.createPipeline(meshBuffer, imageBuffer);

    constexpr auto kWorkgroupSize = 32;
    pipeline.dispatch((kWidth + kWorkgroupSize - 1) / kWorkgroupSize, (kHeight + kWorkgroupSize - 1) / kWorkgroupSize);

    const auto pixels = imageBuffer.map();

    std::unique_ptr<FILE, int (*)(FILE *)> file{ fopen("output.ppm", "wb"), &fclose };
    std::println(file.get(), "P6");
    std::println(file.get(), "{} {}", kWidth, kHeight);
    std::println(file.get(), "255");
    for (const auto pixel : pixels) {
        std::fputc(static_cast<int>(pixel.r * 255.0f), file.get());
        std::fputc(static_cast<int>(pixel.g * 255.0f), file.get());
        std::fputc(static_cast<int>(pixel.b * 255.0f), file.get());
    }
}
