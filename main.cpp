#include <print>

import gpu;

int main()
{
    gpu::Instance instance;

    const auto physDevices = instance.physDevices();
    for (const auto &device : physDevices) {
        std::println("name={}, type={}", device.name(), device.type());
    }
    if (physDevices.empty())
        return 1;

    gpu::Device device = physDevices.front().createDevice();

    constexpr auto kSize = 50;
    gpu::Buffer buffer = device.createBuffer(kSize * sizeof(float));

    gpu::Program program = device.createProgram("shader.spv");
    gpu::Pipeline pipeline = program.createPipeline(buffer);

    constexpr auto kWorkgroupSize = 32;
    pipeline.dispatch((kSize + kWorkgroupSize - 1) / kWorkgroupSize);

    const auto *addr = reinterpret_cast<float *>(buffer.map(0, buffer.size()));
    for (std::size_t i = 0; i < kSize; ++i) {
        std::println("{}: {}", i, addr[i]);
    }
    buffer.unmap();
}
