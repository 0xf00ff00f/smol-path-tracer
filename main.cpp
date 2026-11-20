#include <cassert>
#include <print>
#include <numeric>

import gpu;

template<typename T>
    requires std::is_trivially_copyable_v<T>
std::span<T> spanCast(std::span<std::byte> bytes)
{
    assert(bytes.size() % sizeof(T) == 0);
    assert(reinterpret_cast<std::uintptr_t>(bytes.data()) % alignof(T) == 0);
    const auto size = bytes.size() / sizeof(T);
    return { reinterpret_cast<T *>(bytes.data()), size };
}

int main()
{
    gpu::Instance instance;

    const std::vector<gpu::PhysDevice> physDevices = instance.physDevices();
    for (const auto& device : physDevices)
        std::println("{}: {}", device.name(), device.type());
    if (physDevices.empty())
        return 1;

    gpu::Device device = physDevices.front().createDevice();

    constexpr auto kSize = 50;
    gpu::Buffer inBuffer = device.createBuffer(kSize * sizeof(float));
    gpu::Buffer outBuffer = device.createBuffer(kSize * sizeof(float));

    std::ranges::iota(spanCast<float>(inBuffer.map()), 1);
    inBuffer.unmap();

    gpu::Program program = device.createProgram("simple.spv");
    gpu::Pipeline pipeline = program.createPipeline(inBuffer, outBuffer);

    constexpr auto kWorkgroupSize = 32;
    pipeline.dispatch((kSize + kWorkgroupSize - 1) / kWorkgroupSize);

    std::println("{}", spanCast<const float>(outBuffer.map()));
    outBuffer.unmap();
}
