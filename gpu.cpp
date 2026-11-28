module;

#include <array>
#include <algorithm>
#include <cassert>
#include <experimental/meta>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

export module gpu;

export template<typename T>
    requires std::is_enum_v<T>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(T value, std::format_context &ctx) const
    {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T)))
        {
            if (value == [:e:])
                return std::format_to(ctx.out(), "{}", std::meta::identifier_of(e));
        }
        return std::format_to(ctx.out(), "{}", static_cast<std::underlying_type_t<T>>(value));
    }
};

namespace {

template<typename F, typename... Args>
    requires std::is_invocable_r_v<VkResult, F, Args...>
void checkedVkCall(F &&f, Args &&...args)
{
    const VkResult res = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    if (res != VK_SUCCESS) {
        std::println(stderr, "Failed: {}", res);
        std::abort();
    }
}

std::vector<uint32_t> readFile(const std::string &path)
{
    std::unique_ptr<FILE, int (*)(FILE *)> fp{ fopen(path.c_str(), "rb"), &fclose };
    if (fp == nullptr) {
        std::println(stderr, "Failed to open {}", path);
        return { };
    }

    fseek(fp.get(), 0, SEEK_END);
    const auto size = ftell(fp.get());
    fseek(fp.get(), 0, SEEK_SET);

    const auto sizePadded = ((size + sizeof(uint32_t) - 1) / sizeof(uint32_t)) * sizeof(uint32_t);

    std::vector<uint32_t> bytes(sizePadded, 0);
    fread(bytes.data(), size, 1, fp.get());

    return bytes;
}

} // namespace

export namespace gpu {

class PhysDevice;
class Device;
class Buffer;
class Program;
class Pipeline;

template<typename T>
    requires std::is_trivially_copyable_v<T>
class TypedBuffer;

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

    Device createDevice() const;

private:
    VkPhysicalDevice m_physDevice{ VK_NULL_HANDLE };
};

enum class BufferUsage {
    Uniform,
    Storage,
};

class Device
{
public:
    Device() = default;
    ~Device();

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    Device(Device &&other);
    Device &operator=(Device &&other);

    PhysDevice physDevice() const { return m_physDevice; }
    VkQueue queue() const { return m_queue; }
    VkCommandBuffer commandBuffer() const { return m_commandBuffer; }

    Buffer createBuffer(VkDeviceSize size, BufferUsage usage = BufferUsage::Storage) const;

    template<typename T>
        requires std::is_trivially_copyable_v<T>
    TypedBuffer<T> createTypedBuffer(std::size_t size, BufferUsage usage = BufferUsage::Storage) const
    {
        return TypedBuffer<T>{ this, size, usage };
    }

    Program createProgram(std::string_view path) const;

    operator VkDevice() const { return m_device; }

    explicit operator bool() const { return m_device != VK_NULL_HANDLE; }

private:
    explicit Device(PhysDevice physDevice);
    void create();

    PhysDevice m_physDevice;
    VkDevice m_device{ VK_NULL_HANDLE };
    VkQueue m_queue{ VK_NULL_HANDLE };
    VkCommandPool m_commandPool{ VK_NULL_HANDLE };
    VkCommandBuffer m_commandBuffer{ VK_NULL_HANDLE };

    friend class PhysDevice;
};

class Buffer
{
public:
    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    Buffer(Buffer &&);
    Buffer &operator=(Buffer &&);

    VkDeviceSize sizeBytes() const { return m_sizeBytes; }
    BufferUsage usage() const { return m_usage; }

    std::span<std::byte> mapBytes(VkDeviceSize offset, VkDeviceSize sizeBytes) const;
    std::span<std::byte> mapBytes() const;
    void unmap() const;

    operator VkBuffer() const { return m_buffer; }

    explicit operator bool() const { return m_buffer != VK_NULL_HANDLE; }

protected:
    explicit Buffer(const Device *device, VkDeviceSize size, BufferUsage usage);

private:
    void create();

    const Device *m_device{ nullptr };
    BufferUsage m_usage{ BufferUsage::Storage };
    VkDeviceSize m_sizeBytes{ };
    VkBuffer m_buffer{ VK_NULL_HANDLE };
    VkDeviceMemory m_memory{ VK_NULL_HANDLE };

    friend class Device;
};

template<typename T>
    requires std::is_trivially_copyable_v<T>
class TypedBuffer : public Buffer
{
public:
    TypedBuffer() = default;

    std::span<T> map() const
    {
        const auto bytes = mapBytes();
        assert(bytes.size() == m_size * sizeof(T));
        assert(reinterpret_cast<std::uintptr_t>(bytes.data()) % alignof(T) == 0);
        return { reinterpret_cast<T *>(bytes.data()), m_size };
    }

private:
    explicit TypedBuffer(const Device *device, std::size_t size, BufferUsage usage)
        : Buffer{ device, size * sizeof(T), usage }
        , m_size{ size }
    {
    }

    std::size_t m_size;
    friend class Device;
};

class Program
{
public:
    Program() = default;
    ~Program();

    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;

    Program(Program &&);
    Program &operator=(Program &&);

    std::string_view path() const { return m_path; }

    Pipeline createPipeline() const;

    operator VkShaderModule() const { return m_shaderModule; }

    explicit operator bool() const { return m_shaderModule != VK_NULL_HANDLE; }

private:
    explicit Program(const Device *device, std::string_view path);
    void create();

    const Device *m_device{ nullptr };
    std::string m_path;
    VkShaderModule m_shaderModule{ VK_NULL_HANDLE };

    friend class Device;
};

class Pipeline
{
public:
    Pipeline() = default;
    ~Pipeline();

    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    Pipeline(Pipeline &&);
    Pipeline &operator=(Pipeline &&);

    void bind(uint32_t binding, const Buffer &buffer);

    template<typename T>
        requires std::is_trivially_copyable_v<T>
    void pushConstants(const T &constants);

    void dispatch(uint32_t groupCountX = 1, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

private:
    explicit Pipeline(const Device *device, const Program *program);
    void create();
    void destroy();

    struct BufferBinding {
        uint32_t binding;
        VkBuffer handle;
        VkDeviceSize size;
        BufferUsage usage;
    };

    bool m_isDirty{ true };
    const Device *m_device{ nullptr };
    const Program *m_program{ nullptr };
    std::vector<BufferBinding> m_bufferBindings;
    std::optional<std::span<const std::byte>> m_pushConstants;
    VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSet m_descriptorSet{ VK_NULL_HANDLE };
    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline m_pipeline{ VK_NULL_HANDLE };

    friend class Program;
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

    checkedVkCall(vkCreateInstance, &createInfo, nullptr, &m_instance);
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

Device PhysDevice::createDevice() const
{
    return Device{ *this };
}

Device::Device(PhysDevice physDevice)
    : m_physDevice{ physDevice }
{
    create();
}

Device::~Device()
{
    if (m_commandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    if (m_device != VK_NULL_HANDLE)
        vkDestroyDevice(m_device, nullptr);
}

Device::Device(Device &&other)
    : m_physDevice{ std::exchange(other.m_physDevice, { }) }
    , m_device{ std::exchange(other.m_device, VK_NULL_HANDLE) }
    , m_queue{ std::exchange(other.m_queue, VK_NULL_HANDLE) }
    , m_commandPool{ std::exchange(other.m_commandPool, VK_NULL_HANDLE) }
    , m_commandBuffer{ std::exchange(other.m_commandBuffer, VK_NULL_HANDLE) }
{
}

Device &Device::operator=(Device &&other)
{
    if (this != &other) {
        if (m_commandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        if (m_device != VK_NULL_HANDLE)
            vkDestroyDevice(m_device, nullptr);
        m_physDevice = std::exchange(other.m_physDevice, { });
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_queue = std::exchange(other.m_queue, VK_NULL_HANDLE);
        m_commandPool = std::exchange(other.m_commandPool, VK_NULL_HANDLE);
        m_commandBuffer = std::exchange(other.m_commandBuffer, VK_NULL_HANDLE);
    }
    return *this;
}

void Device::create()
{
    std::optional<uint32_t> queueFamilyIndex = m_physDevice.computeQueueFamilyIndex();
#if 0
    // disabled, libc++ in clang-p2996 doesn't have std::optional range support :(
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
#else
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    if (queueFamilyIndex.has_value()) {
        static float queuePriorities = 1.0f;
        queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = *queueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriorities });
    }
#endif

    VkPhysicalDeviceFeatures deviceFeatures = { };

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .pEnabledFeatures = &deviceFeatures
    };

    checkedVkCall(vkCreateDevice, m_physDevice, &createInfo, nullptr, &m_device);

    if (queueFamilyIndex.has_value()) {
        vkGetDeviceQueue(m_device, *queueFamilyIndex, 0, &m_queue);

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = *queueFamilyIndex
        };
        checkedVkCall(vkCreateCommandPool, m_device, &commandPoolCreateInfo, nullptr, &m_commandPool);

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        checkedVkCall(vkAllocateCommandBuffers, m_device, &commandBufferAllocateInfo, &m_commandBuffer);
    }
}

Buffer Device::createBuffer(VkDeviceSize size, BufferUsage usage) const
{
    return Buffer{ this, size, usage };
}

Program Device::createProgram(std::string_view path) const
{
    return Program{ this, path };
}

Buffer::Buffer(const Device *device, VkDeviceSize sizeBytes, BufferUsage usage)
    : m_device{ device }
    , m_sizeBytes{ sizeBytes }
    , m_usage{ usage }
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
    , m_sizeBytes{ std::exchange(other.m_sizeBytes, 0) }
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
        m_sizeBytes = std::exchange(other.m_sizeBytes, 0);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
    }
    return *this;
}

VkBufferUsageFlagBits toBufferUsageFlagBits(BufferUsage usage)
{
    switch (usage) {
    case BufferUsage::Uniform:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case BufferUsage::Storage:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    default:
        return static_cast<VkBufferUsageFlagBits>(0);
    }
}

void Buffer::create()
{
    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = m_sizeBytes,
        .usage = toBufferUsageFlagBits(m_usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    checkedVkCall(vkCreateBuffer, *m_device, &createInfo, nullptr, &m_buffer);

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
        checkedVkCall(vkAllocateMemory, *m_device, &allocateInfo, nullptr, &m_memory);
        checkedVkCall(vkBindBufferMemory, *m_device, m_buffer, m_memory, 0);
    }
}

std::span<std::byte> Buffer::mapBytes(VkDeviceSize offset, VkDeviceSize size) const
{
    void *mappedMemory;
    checkedVkCall(vkMapMemory, *m_device, m_memory, offset, size, 0, &mappedMemory);
    return { static_cast<std::byte *>(mappedMemory), size };
}

std::span<std::byte> Buffer::mapBytes() const
{
    return mapBytes(0, m_sizeBytes);
}

void Buffer::unmap() const
{
    vkUnmapMemory(*m_device, m_memory);
}

Program::Program(const Device *device, std::string_view path)
    : m_device{ device }
    , m_path{ path }
{
    create();
}

Program::~Program()
{
    if (m_shaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(*m_device, m_shaderModule, nullptr);
}

Program::Program(Program &&other)
    : m_device{ std::exchange(other.m_device, nullptr) }
    , m_path{ std::move(other.m_path) }
    , m_shaderModule{ std::exchange(other.m_shaderModule, VK_NULL_HANDLE) }
{
}

Program &Program::operator=(Program &&other)
{
    if (this != &other) {
        if (m_shaderModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(*m_device, m_shaderModule, nullptr);
        m_device = std::exchange(other.m_device, nullptr);
        m_path = std::move(other.m_path);
        m_shaderModule = std::exchange(other.m_shaderModule, VK_NULL_HANDLE);
    }
    return *this;
}

void Program::create()
{
    const std::vector<uint32_t> code = readFile(m_path.c_str());
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = static_cast<uint32_t>(code.size()),
        .pCode = code.data()
    };
    checkedVkCall(vkCreateShaderModule, *m_device, &createInfo, nullptr, &m_shaderModule);
}

Pipeline Program::createPipeline() const
{
    return Pipeline{ m_device, this };
}

Pipeline::Pipeline(const Device *device, const Program *program)
    : m_device{ device }
    , m_program{ program }
{
}

VkDescriptorType toDescriptorType(BufferUsage usage)
{
    switch (usage) {
    case BufferUsage::Uniform:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case BufferUsage::Storage:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    default:
        return static_cast<VkDescriptorType>(0);
    }
}

void Pipeline::create()
{
    // create descriptor set layout

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(m_bufferBindings.size());

    for (const auto &bufferBinding : m_bufferBindings) {
        bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = bufferBinding.binding,
                .descriptorType = toDescriptorType(bufferBinding.usage),
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT });
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    checkedVkCall(vkCreateDescriptorSetLayout, *m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout);

    // allocate descriptor set

    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto usage : { BufferUsage::Storage, BufferUsage::Uniform }) {
        const auto count = std::ranges::count(m_bufferBindings, usage, &BufferBinding::usage);
        if (count > 0) {
            poolSizes.push_back(
                    VkDescriptorPoolSize{
                            .type = toDescriptorType(usage),
                            .descriptorCount = static_cast<uint32_t>(count),
                    });
        }
    }

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    checkedVkCall(vkCreateDescriptorPool, *m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout
    };

    checkedVkCall(vkAllocateDescriptorSets, *m_device, &descriptorSetAllocateInfo, &m_descriptorSet);

    // update descriptor set

    std::vector<std::unique_ptr<VkDescriptorBufferInfo>> bufferInfos;
    bufferInfos.reserve(m_bufferBindings.size());

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(m_bufferBindings.size());

    for (const auto &bufferBinding : m_bufferBindings) {
        auto bufferInfo = std::make_unique<VkDescriptorBufferInfo>(VkDescriptorBufferInfo{
                .buffer = bufferBinding.handle,
                .offset = 0,
                .range = bufferBinding.size });

        descriptorWrites.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptorSet,
                .dstBinding = bufferBinding.binding,
                .descriptorCount = 1,
                .descriptorType = toDescriptorType(bufferBinding.usage),
                .pBufferInfo = bufferInfo.get() });

        bufferInfos.push_back(std::move(bufferInfo));
    }

    vkUpdateDescriptorSets(*m_device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

    // create pipeline layout

    std::vector<VkPushConstantRange> pushConstantRanges;
    if (m_pushConstants.has_value()) {
        pushConstantRanges.push_back(VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = static_cast<uint32_t>(m_pushConstants->size()),
        });
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data()
    };
    checkedVkCall(vkCreatePipelineLayout, *m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

    // create pipeline

    VkComputePipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = *m_program,
                .pName = "main" },
        .layout = m_pipelineLayout
    };

    checkedVkCall(vkCreateComputePipelines, *m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline);
}

void Pipeline::destroy()
{
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(*m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(*m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(*m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(*m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

Pipeline::~Pipeline()
{
    destroy();
}

Pipeline::Pipeline(Pipeline &&other)
    : m_device{ std::exchange(other.m_device, nullptr) }
    , m_program{ std::exchange(other.m_program, nullptr) }
    , m_bufferBindings{ std::move(other.m_bufferBindings) }
    , m_descriptorSetLayout{ std::exchange(other.m_descriptorSetLayout, VK_NULL_HANDLE) }
    , m_descriptorPool{ std::exchange(other.m_descriptorPool, VK_NULL_HANDLE) }
    , m_descriptorSet{ std::exchange(other.m_descriptorSet, VK_NULL_HANDLE) }
    , m_pipelineLayout{ std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE) }
    , m_pipeline{ std::exchange(other.m_pipeline, VK_NULL_HANDLE) }
{
}

Pipeline &Pipeline::operator=(Pipeline &&other)
{
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, nullptr);
        m_program = std::exchange(other.m_program, nullptr);
        m_bufferBindings = std::move(other.m_bufferBindings);
        m_descriptorSetLayout = std::exchange(other.m_descriptorSetLayout, VK_NULL_HANDLE);
        m_descriptorPool = std::exchange(other.m_descriptorPool, VK_NULL_HANDLE);
        m_descriptorSet = std::exchange(other.m_descriptorSet, VK_NULL_HANDLE);
        m_pipelineLayout = std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE);
        m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
    }
    return *this;
}

template<typename T>
    requires std::is_trivially_copyable_v<T>
void Pipeline::pushConstants(const T &constants)
{
    const auto prevSize = m_pushConstants.transform([](const std::span<const std::byte> &constants) { return constants.size(); }).value_or(0);
    m_pushConstants = std::as_bytes(std::span{ std::addressof(constants), 1 });
    if (prevSize != sizeof(T))
        m_isDirty = true;
}

void Pipeline::bind(uint32_t binding, const Buffer &buffer)
{
    const auto alreadyBound = std::ranges::any_of(m_bufferBindings, [binding, &buffer](const BufferBinding &bufferBinding) {
        return bufferBinding.binding == binding && bufferBinding.usage == buffer.usage();
    });
    if (alreadyBound) {
        std::println(stderr, "{} already bound to {}", buffer.usage(), binding);
    }
    m_bufferBindings.push_back(BufferBinding{
            .binding = binding,
            .handle = static_cast<VkBuffer>(buffer),
            .size = buffer.sizeBytes(),
            .usage = buffer.usage() });
    m_isDirty = true;
}

void Pipeline::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    if (m_isDirty) {
        create();
        m_isDirty = false;
    }

    const auto commandBuffer = m_device->commandBuffer();

    // record command buffer

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    checkedVkCall(vkBeginCommandBuffer, commandBuffer, &beginInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    if (m_pushConstants.has_value()) {
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_pushConstants->size(), m_pushConstants->data());
    }
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

    checkedVkCall(vkEndCommandBuffer, commandBuffer);

    // submit

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0
    };
    checkedVkCall(vkCreateFence, *m_device, &fenceCreateInfo, nullptr, &fence);

    checkedVkCall(vkQueueSubmit, m_device->queue(), 1, &submitInfo, fence);
    checkedVkCall(vkWaitForFences, *m_device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    vkDestroyFence(*m_device, fence, nullptr);
}

} // namespace gpu
