#include <cassert>
#include <print>
#include <numeric>

import gpu;

#ifdef USE_RENDERDOC
import renderdoc;
#endif

int main()
{
    gpu::Instance instance;

    const std::vector<gpu::PhysDevice> physDevices = instance.physDevices();
    for (const auto &physDevice : physDevices) {
        std::println("Device: {} ({})", physDevice.name(), physDevice.type());

        gpu::Device device = physDevice.createDevice();

#ifdef USE_RENDERDOC
        renderdoc::FrameCapture capture(physDevice.name().c_str());
#endif

        // std140
        struct alignas(16) Float140 {
            explicit Float140(float v) : value{ v } { };
            operator float() const { return value; }
            Float140& operator=(float v) { value = v; return *this; }
            float value;
        };

        const uint32_t bufferSize = 50;
        gpu::TypedBuffer<Float140> inBuffer = device.createTypedBuffer<Float140>(bufferSize, gpu::BufferUsage::Uniform);
        gpu::TypedBuffer<float> outBuffer = device.createTypedBuffer<float>(bufferSize);

        auto data = inBuffer.map();
        std::iota(data.begin(), data.end(), 1.0f);
        inBuffer.unmap();

        gpu::Program program = device.createProgram("simple.comp.spv");

        gpu::Pipeline pipeline = program.createPipeline();
        pipeline.bind(0, inBuffer);
        pipeline.bind(1, outBuffer);
        pipeline.pushConstants(bufferSize);

        constexpr auto kWorkgroupSize = 32;
        pipeline.dispatch((bufferSize + kWorkgroupSize - 1) / kWorkgroupSize);

        std::println("{}", outBuffer.map());
        outBuffer.unmap();
    }
}
