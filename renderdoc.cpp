module;

#include <dlfcn.h>
#include <cassert>

#include <renderdoc_app.h>

export module renderdoc;

namespace renderdoc {

static RENDERDOC_API_1_7_0 *getAPI()
{
    static auto *api = [] {
        RENDERDOC_API_1_7_0 *api{ nullptr };
        if (auto *handle = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
            auto *getAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(handle, "RENDERDOC_GetAPI"));
            auto result = getAPI(eRENDERDOC_API_Version_1_7_0, reinterpret_cast<void **>(&api));
            assert(result == 1);
        }
        return api;
    }();
    return api;
}

export class FrameCapture
{
public:
    explicit FrameCapture(const char *title)
    {
        if (auto *renderDoc = getAPI()) {
            renderDoc->StartFrameCapture(nullptr, nullptr);
            renderDoc->SetCaptureTitle(title);
        }
    }

    ~FrameCapture()
    {
        if (auto *renderDoc = getAPI()) {
            renderDoc->EndFrameCapture(nullptr, nullptr);
        }
    }

    FrameCapture(const FrameCapture &) = delete;
    FrameCapture &operator=(const FrameCapture &) = delete;

    FrameCapture(FrameCapture &&) = delete;
    FrameCapture &operator=(FrameCapture &&) = delete;
};

} // namespace renderdoc
