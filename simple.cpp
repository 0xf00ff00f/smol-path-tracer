#include <cassert>
#include <print>
#include <numeric>

import gpu;

int main()
{
    gpu::Instance instance;

    const std::vector<gpu::PhysDevice> physDevices = instance.physDevices();
    for (const auto &physDevice : physDevices) {
        std::println("Device: {} ({})", physDevice.name(), physDevice.type());

        gpu::Device device = physDevice.createDevice();

        constexpr auto kSize = 50;
        gpu::TypedBuffer<float> inBuffer = device.createTypedBuffer<float>(kSize);
        gpu::TypedBuffer<float> outBuffer = device.createTypedBuffer<float>(kSize);

        std::ranges::iota(inBuffer.map(), 1);
        inBuffer.unmap();

        gpu::Program program = device.createProgram("simple.comp.spv");
        gpu::Pipeline pipeline = program.createPipeline(inBuffer, outBuffer);

        constexpr auto kWorkgroupSize = 32;
        pipeline.dispatch((kSize + kWorkgroupSize - 1) / kWorkgroupSize);

        std::println("{}", outBuffer.map());
        outBuffer.unmap();
    }
}
