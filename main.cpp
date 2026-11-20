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

    gpu::Device device(physDevices.front());

    constexpr auto kSize = 50;
    gpu::Buffer buffer(&device, kSize * sizeof(float));

    gpu::Program program(&device, "shader.spv");
    gpu::Pipeline pipeline(&device, &program, buffer);

    constexpr auto kWorkgroupSize = 32;
    pipeline.dispatch((kSize + kWorkgroupSize - 1) / kWorkgroupSize, 1, 1);

    const auto *addr = reinterpret_cast<float *>(buffer.map(0, buffer.size()));
    for (std::size_t i = 0; i < kSize; ++i) {
        std::println("{}: {}", i, addr[i]);
    }
    buffer.unmap();
}
