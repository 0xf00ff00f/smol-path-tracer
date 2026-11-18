#include <optional>
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

class PhysDevice;
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

    std::vector<PhysDevice> physDevices() const;

    operator VkInstance() const { return m_instance; }

private:
    void create();

    VkInstance m_instance{ VK_NULL_HANDLE };
};

class PhysDevice
{
public:
    enum class Type {
        Other = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER,
        IntegratedGPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        DiscreteGPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        VirtualGPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
        CPU = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU,
    };

    PhysDevice() = default;
    explicit PhysDevice(VkPhysicalDevice physDevice);

    operator VkPhysicalDevice() const { return m_physDevice; }

    Type type() const;
    std::string name() const;

    std::optional<uint32_t> computeQueueFamilyIndex() const;

private:
    VkPhysicalDevice m_physDevice{ VK_NULL_HANDLE };
};

class Device
{
public:
    explicit Device(PhysDevice physDevice);
    ~Device();

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    Device(Device &&other);
    Device &operator=(Device &&other);

    PhysDevice physDevice() const { return m_physDevice; }

    operator VkDevice() const { return m_device; }

private:
    void create();

    PhysDevice m_physDevice;
    VkDevice m_device{ VK_NULL_HANDLE };
};

class Buffer
{
public:
    explicit Buffer(const Device *device, VkDeviceSize size);
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    Buffer(Buffer &&);
    Buffer &operator=(Buffer &&);

    VkDeviceSize size() const { return m_size; }

    operator VkBuffer() const { return m_buffer; }

private:
    void create();

    const Device *m_device{ nullptr };
    VkDeviceSize m_size{ };
    VkBuffer m_buffer{ VK_NULL_HANDLE };
    VkDeviceMemory m_memory{ VK_NULL_HANDLE };
};

Instance::Instance()
{
    create();
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

void Instance::create()
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

std::vector<PhysDevice> Instance::physDevices() const
{
    uint32_t count;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    std::vector<VkPhysicalDevice> physDevices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, physDevices.data());

    return physDevices
            | std::views::transform([](const VkPhysicalDevice physDevice) { return PhysDevice{ physDevice }; })
            | std::ranges::to<std::vector>();
}

PhysDevice::PhysDevice(VkPhysicalDevice physDevice)
    : m_physDevice{ physDevice }
{
}

PhysDevice::Type PhysDevice::type() const
{
    VkPhysicalDeviceProperties properties = { };
    vkGetPhysicalDeviceProperties(m_physDevice, &properties);
    return static_cast<PhysDevice::Type>(properties.deviceType);
}

std::string PhysDevice::name() const
{
    VkPhysicalDeviceProperties properties = { };
    vkGetPhysicalDeviceProperties(m_physDevice, &properties);
    return properties.deviceName;
}

std::optional<uint32_t> PhysDevice::computeQueueFamilyIndex() const
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &count, queueFamilies.data());

    auto it = std::ranges::find_if(queueFamilies, [](const VkQueueFamilyProperties &props) {
        return props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT);
    });
    if (it == queueFamilies.end())
        return std::nullopt;

    return std::distance(queueFamilies.begin(), it);
}

Device::Device(PhysDevice physDevice)
    : m_physDevice{ physDevice }
{
    create();
}

Device::~Device()
{
    if (m_device != VK_NULL_HANDLE)
        vkDestroyDevice(m_device, nullptr);
}

Device::Device(Device &&other)
    : m_physDevice{ std::exchange(other.m_physDevice, { }) }
    , m_device{ std::exchange(other.m_device, VK_NULL_HANDLE) }
{
}

Device &Device::operator=(Device &&other)
{
    if (this != &other) {
        if (m_device != VK_NULL_HANDLE)
            vkDestroyDevice(m_device, nullptr);
        m_physDevice = std::exchange(other.m_physDevice, { });
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }
    return *this;
}

void Device::create()
{
    std::optional<uint32_t> queueFamilyIndex = m_physDevice.computeQueueFamilyIndex();
    const std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = queueFamilyIndex | std::views::transform([](uint32_t queueFamilyIndex) {
                                                                      static float queuePriorities = 1.0f;
                                                                      return VkDeviceQueueCreateInfo{
                                                                          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                                          .queueFamilyIndex = queueFamilyIndex,
                                                                          .queueCount = 1,
                                                                          .pQueuePriorities = &queuePriorities
                                                                      };
                                                                  })
            | std::ranges::to<std::vector>();

    VkPhysicalDeviceFeatures deviceFeatures = { };

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .pEnabledFeatures = &deviceFeatures
    };

    vkCheckedCall(vkCreateDevice, m_physDevice, &createInfo, nullptr, &m_device);
}

Buffer::Buffer(const Device *device, VkDeviceSize size)
    : m_device{ device }
    , m_size{ size }
{
    create();
}

Buffer::~Buffer()
{
    if (m_memory != VK_NULL_HANDLE)
        vkFreeMemory(*m_device, m_memory, nullptr);

    if (m_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(*m_device, m_buffer, nullptr);
}

Buffer::Buffer(Buffer &&other)
    : m_device{ std::exchange(other.m_device, nullptr) }
    , m_size{ std::exchange(other.m_size, 0) }
    , m_buffer{ std::exchange(other.m_buffer, VK_NULL_HANDLE) }
    , m_memory{ std::exchange(other.m_memory, VK_NULL_HANDLE) }
{
}

Buffer &Buffer::operator=(Buffer &&other)
{
    if (this != &other) {
        if (m_memory != VK_NULL_HANDLE)
            vkFreeMemory(*m_device, m_memory, nullptr);
        if (m_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(*m_device, m_buffer, nullptr);
        m_device = std::exchange(other.m_device, nullptr);
        m_size = std::exchange(other.m_size, 0);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
    }
    return *this;
}

void Buffer::create()
{
    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = m_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    vkCheckedCall(vkCreateBuffer, *m_device, &createInfo, nullptr, &m_buffer);

    VkMemoryRequirements memoryRequirements = { };
    vkGetBufferMemoryRequirements(*m_device, m_buffer, &memoryRequirements);

    const auto memoryTypeIndex = [this, memoryTypeBits = memoryRequirements.memoryTypeBits]() -> std::optional<uint32_t> {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(m_device->physDevice(), &memoryProperties);
        constexpr uint32_t kFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            if (memoryTypeBits & (1 << i)) {
                const auto flags = memoryProperties.memoryTypes[i].propertyFlags;
                if ((flags & kFlags) == kFlags) {
                    return i;
                }
            }
        }
        return std::nullopt;
    }();

    if (memoryTypeIndex.has_value()) {
        VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = *memoryTypeIndex
        };
        vkCheckedCall(vkAllocateMemory, *m_device, &allocateInfo, nullptr, &m_memory);
        vkCheckedCall(vkBindBufferMemory, *m_device, m_buffer, m_memory, 0);
    }
}

} // namespace v

int main()
{
    v::Instance instance;
    const auto physDevices = instance.physDevices();
    for (const auto &device : physDevices) {
        std::println("name={}, type={}", device.name(), static_cast<int>(device.type()));
    }
    if (!physDevices.empty()) {
        v::Device device(physDevices.front());
        v::Buffer buffer(&device, 2048);
    }
}
