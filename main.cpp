#include <vulkan/vulkan.h>

#include <utility>
#include <array>
#include <print>

namespace v {

namespace {

template<typename F, typename... Args>
    requires std::is_invocable_r_v<VkResult, F, Args...>
void vkCheckResult(F &&f, Args &&...args)
{
    const VkResult res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    if (res != VK_SUCCESS) {
        std::println(stderr, "Failed: {}", static_cast<std::underlying_type_t<VkResult>>(res));
        std::abort();
    }
}

} // namespace

class Instance
{
public:
    explicit Instance();
    ~Instance();

    Instance(const Instance &) = delete;
    Instance &operator=(Instance &) = delete;

    Instance(Instance &&other);
    Instance &operator=(Instance &&other);

private:
    void createInstance();

    VkInstance m_handle;
};

Instance::Instance()
{
    createInstance();
}

Instance::~Instance()
{
    if (m_handle != VK_NULL_HANDLE)
        vkDestroyInstance(m_handle, nullptr);
}

Instance::Instance(Instance &&other)
    : m_handle{ std::exchange(other.m_handle, VK_NULL_HANDLE) }
{
}

Instance &Instance::operator=(Instance &&other)
{
    if (this != &other) {
        if (m_handle != VK_NULL_HANDLE)
            vkDestroyInstance(m_handle, nullptr);
        m_handle = std::exchange(other.m_handle, VK_NULL_HANDLE);
    }
    return *this;
}

void Instance::createInstance()
{
    constexpr std::array layers = { "VK_LAYER_KHRONOS_validation" };
    constexpr std::array extensions = { VK_EXT_DEBUG_REPORT_EXTENSION_NAME };

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Compute shader test",
        .applicationVersion = 0,
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = layers.size(),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = extensions.size(),
        .ppEnabledExtensionNames = extensions.data()
    };

    vkCheckResult(vkCreateInstance, &createInfo, nullptr, &m_handle);
}

} // namespace v

int main()
{
    v::Instance instance;
}
