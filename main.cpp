#include <vulkan/vulkan.h>

#include <utility>
#include <array>
#include <print>
#include <vector>
#include <ranges>

namespace v {

namespace {

template<typename F, typename... Args>
    requires std::is_invocable_r_v<VkResult, F, Args...>
void vkCheckedCall(F &&f, Args &&...args)
{
    const VkResult res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    if (res != VK_SUCCESS) {
        std::println(stderr, "Failed: {}", static_cast<std::underlying_type_t<VkResult>>(res));
        std::abort();
    }
}

} // namespace

class Device;

class Instance
{
public:
    explicit Instance();
    ~Instance();

    Instance(const Instance &) = delete;
    Instance &operator=(Instance &) = delete;

    Instance(Instance &&other);
    Instance &operator=(Instance &&other);

    std::vector<Device> devices() const;

private:
    void createInstance();

    VkInstance m_instance;
};

class Device
{
public:
    enum class Type {
        Other = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER,
        IntegratedGPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        DiscreteGPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        VirtualGPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
        CPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU,
    };

    explicit Device(VkPhysicalDevice physDevice);
    ~Device();

    Type type() const;
    std::string name() const;

private:
    VkPhysicalDevice m_physDevice;
};

Instance::Instance()
{
    createInstance();
}

Instance::~Instance()
{
    if (m_instance != VK_NULL_HANDLE)
        vkDestroyInstance(m_instance, nullptr);
}

Instance::Instance(Instance &&other)
    : m_instance{ std::exchange(other.m_instance, VK_NULL_HANDLE) }
{
}

Instance &Instance::operator=(Instance &&other)
{
    if (this != &other) {
        if (m_instance != VK_NULL_HANDLE)
            vkDestroyInstance(m_instance, nullptr);
        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
    }
    return *this;
}

void Instance::createInstance()
{
    constexpr std::array layers = { "VK_LAYER_KHRONOS_validation" };

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
    };

    vkCheckedCall(vkCreateInstance, &createInfo, nullptr, &m_instance);
}

std::vector<Device> Instance::devices() const
{
    uint32_t count;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    std::vector<VkPhysicalDevice> physDevices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, physDevices.data());

    return physDevices
            | std::views::transform([](const VkPhysicalDevice physDevice) { return Device{ physDevice }; })
            | std::ranges::to<std::vector>();
}

Device::Device(VkPhysicalDevice physDevice)
    : m_physDevice{ physDevice }
{
}

Device::~Device() = default;

Device::Type Device::type() const
{
    VkPhysicalDeviceProperties properties = { };
    vkGetPhysicalDeviceProperties(m_physDevice, &properties);
    return static_cast<Device::Type>(properties.deviceType);
}

std::string Device::name() const
{
    VkPhysicalDeviceProperties properties = { };
    vkGetPhysicalDeviceProperties(m_physDevice, &properties);
    return properties.deviceName;
}

} // namespace v

int main()
{
    v::Instance instance;
    const auto devices = instance.devices();
    for (const auto &device : devices) {
        std::println("name={}, type={}", device.name(), static_cast<int>(device.type()));
    }
}
