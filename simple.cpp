#include <cassert>
#include <print>
#include <numeric>

#ifdef USE_RENDERDOC
#include <dlfcn.h>
#include <renderdoc_app.h>
#endif

import gpu;

int main()
{
    gpu::Instance instance;

    const std::vector<gpu::PhysDevice> physDevices = instance.physDevices();
    for (const auto &physDevice : physDevices) {
        std::println("Device: {} ({})", physDevice.name(), physDevice.type());

        gpu::Device device = physDevice.createDevice();

#ifdef USE_RENDERDOC
        RENDERDOC_API_1_7_0 *renderDoc = nullptr;
        if (auto* handle = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
        {
            auto *getAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(handle, "RENDERDOC_GetAPI"));
            auto result = getAPI(eRENDERDOC_API_Version_1_7_0, reinterpret_cast<void **>(&renderDoc));
            assert(result == 1);
        }

        if (renderDoc) {
            renderDoc->StartFrameCapture(nullptr, nullptr);
            renderDoc->SetCaptureTitle(physDevice.name().c_str());
        }
#endif

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

#ifdef USE_RENDERDOC
        if (renderDoc)
            renderDoc->EndFrameCapture(nullptr, nullptr);
#endif
    }
}
