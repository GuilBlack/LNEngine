#include "GfxContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

#include "Shader.h"
#include "Core/ApplicationBase.h"
#include "Engine/Graphics/Texture.h"
#include "CommandBufferManager.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

constexpr bool DEBUG_SHADER_PRINTF_CALLBACK = false;

namespace lne
{
bool GfxContext::s_DynamicLoaderInitialized = false;
vk::Instance GfxContext::s_VulkanInstance{nullptr};
vkb::Instance GfxContext::s_VkbInstance{};

GfxContext::GfxContext(vk::SurfaceKHR surface)
{
    LNE_ASSERT(s_VulkanInstance, "You should call InitVulkan before trying to create a window!");

    // Select a physical device
    auto vkbPhysicalDevice = VkbSelectPhysicalDevice(s_VkbInstance, surface);

    m_PhysicalDevice = vkbPhysicalDevice.physical_device;
    m_EnabledFeatures = vkbPhysicalDevice.features;
    m_Properties = m_PhysicalDevice.getProperties();
    m_MemoryProperties = vkbPhysicalDevice.memory_properties;
    m_QueueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();

    // Create a logical device
    auto deviceRet = vkb::DeviceBuilder{ vkbPhysicalDevice }.build();
    LNE_ASSERT(deviceRet, "Failed to create device");
    auto deviceVal = deviceRet.value();
    m_Device = deviceRet.value().device;

    // Get queues
    auto computeFamilyRet = deviceVal.get_dedicated_queue_index(vkb::QueueType::compute);
    if (!computeFamilyRet)
    {
        computeFamilyRet = deviceVal.get_queue_index(vkb::QueueType::compute);
        m_QueueFamilyIndices.ComputeFamily = computeFamilyRet.value();
        m_ComputeQueue = deviceVal.get_queue(vkb::QueueType::compute).value();
    }
    else
    {
        m_QueueFamilyIndices.ComputeFamily = computeFamilyRet.value();
        m_ComputeQueue = deviceVal.get_dedicated_queue(vkb::QueueType::compute).value();
    }

    auto transferFamilyRet = deviceVal.get_dedicated_queue_index(vkb::QueueType::transfer);
    if (!transferFamilyRet)
    {
        transferFamilyRet = deviceVal.get_queue_index(vkb::QueueType::transfer);
        m_QueueFamilyIndices.TransferFamily = transferFamilyRet.value();
        m_TransferQueue = deviceVal.get_queue(vkb::QueueType::transfer).value();
    }
    else
    {
        m_QueueFamilyIndices.TransferFamily = transferFamilyRet.value();
        m_TransferQueue = deviceVal.get_dedicated_queue(vkb::QueueType::transfer).value();
    }

    m_QueueFamilyIndices.GraphicsFamily = deviceVal.get_queue_index(vkb::QueueType::graphics).value();
    m_GraphicsQueue = deviceVal.get_queue(vkb::QueueType::graphics).value();
    m_QueueFamilyIndices.PresentFamily = deviceVal.get_queue_index(vkb::QueueType::present).value();
    m_PresentQueue = deviceVal.get_queue(vkb::QueueType::present).value();

    LNE_ASSERT(m_QueueFamilyIndices.IsComplete(), "Failed to find all queue families");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);

    SetVkObjectName(m_PhysicalDevice, "PhysicalDevice");
    SetVkObjectName(m_Device, "Device");
    CreateMemoryAllocator();

    m_TransferCommandBufferManager.reset(lnnew CommandBufferManager(this, 1, EQueueFamilyType::Transfer));

#pragma region Bindless
    static constexpr uint32_t bindlessPoolSize = 2048;

    std::vector<vk::DescriptorPoolSize> poolSizesBindless{
        vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, bindlessPoolSize },
    };

    vk::DescriptorPoolCreateInfo poolInfoBindless{
        vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
        bindlessPoolSize* (uint32_t)poolSizesBindless.size(),
        poolSizesBindless
    };

    m_BindlessDescriptorPool = m_Device.createDescriptorPool(poolInfoBindless);
    SetVkObjectName(m_BindlessDescriptorPool, "BindlessDescriptorPool");

    vk::DescriptorSetLayoutBinding bindlessLayoutBinding = vk::DescriptorSetLayoutBinding{
        0,
        vk::DescriptorType::eCombinedImageSampler,
        bindlessPoolSize,
        vk::ShaderStageFlagBits::eAll
    };

    std::array<vk::DescriptorBindingFlags, 1> bindlessBindingFlags{
        vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound
    };

    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindlessLayoutBindingFlags{
        bindlessBindingFlags
    };

    vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> bindlessLayoutChain{
        vk::DescriptorSetLayoutCreateInfo{
            vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            bindlessLayoutBinding
        },
        bindlessLayoutBindingFlags
    };

    m_BindlessDescriptorSetLayout = m_Device.createDescriptorSetLayout(bindlessLayoutChain.get<vk::DescriptorSetLayoutCreateInfo>());
    SetVkObjectName(m_BindlessDescriptorSetLayout, "BindlessDescriptorSetLayout");

    vk::DescriptorSetAllocateInfo allocInfoBindless{
        m_BindlessDescriptorPool,
        m_BindlessDescriptorSetLayout
    };
    auto result = m_Device.allocateDescriptorSets(allocInfoBindless);
    m_BindlessDescriptorSet = result.back();
    SetVkObjectName(m_BindlessDescriptorSet, "BindlessDescriptorSet");

    m_FreeBindlessIndices = std::queue<uint32_t>();
    for (uint32_t i = 0; i < bindlessPoolSize; i++)
        m_FreeBindlessIndices.push(i);
#pragma endregion
}

GfxContext::~GfxContext()
{
    m_TransferCommandBufferManager.reset();
    m_Device.resetDescriptorPool(m_BindlessDescriptorPool);
    m_Device.destroyDescriptorPool(m_BindlessDescriptorPool);
    m_Device.destroyDescriptorSetLayout(m_BindlessDescriptorSetLayout);
    vmaDestroyAllocator(m_MemoryAllocator);
    m_Device.destroy();
}

bool GfxContext::InitVulkan(std::string appName)
{
    if (glfwVulkanSupported() == GLFW_FALSE)
    {
        LNE_ERROR("Vulkan not supported by GLFW");
        return false;
    }

    if (s_VulkanInstance)
    {
        LNE_ERROR("Vulkan instance already initialized");
        return false;
    }
    if (!s_DynamicLoaderInitialized)
    {
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        s_DynamicLoaderInitialized = true;
    }
    vkb::SystemInfo sysInfo = vkb::SystemInfo::get_system_info().value();
    std::vector<std::string> availableLayers;
    for (auto& validationLayer : sysInfo.available_layers)
    {
        availableLayers.push_back(validationLayer.layerName);
    }
    std::vector<std::string> requiredLayers = { "VK_LAYER_KHRONOS_validation" };
    std::vector<std::string> instanceLayers = vkut::FilterExtensions(requiredLayers, availableLayers);

    auto instanceBuilder = vkb::InstanceBuilder()
        .set_debug_callback(&GfxContext::DebugPrintfCallback)
        .set_engine_name("LNEngine")
        .set_app_name(appName.c_str())
        .require_api_version(1, 3, 0);

    instanceBuilder.add_debug_messenger_type(
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
    #if defined(VK_EXT_device_address_binding_report)
        | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
    #endif
    );

    for (const auto& layer : instanceLayers)
        instanceBuilder.enable_layer(layer.c_str());

    std::vector<std::string> availableExtensions;
    for (auto& extension : sysInfo.available_extensions)
        availableExtensions.push_back(extension.extensionName);
    std::vector<std::string> requiredExtensions = {
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    std::vector<std::string> instanceExtensions = vkut::FilterExtensions(requiredExtensions,
        availableExtensions
    );

    for (const auto& extension : instanceExtensions)
        instanceBuilder.enable_extension(extension.c_str());

    auto instRet = instanceBuilder.build();
    if (!instRet)
    {
        LNE_ERROR("Failed to create Vulkan instance: {0}", instRet.error().message());
        return false;
    }
    s_VulkanInstance = vk::Instance(instRet.value().instance);
    s_VkbInstance = instRet.value();

    VULKAN_HPP_DEFAULT_DISPATCHER.init(s_VulkanInstance);
    return true;
}

void GfxContext::NukeVulkan()
{
    s_VulkanInstance.destroy();
    s_VulkanInstance = nullptr;
}

void GfxContext::InitDefaultResources()
{
    m_DefaultSampler = CreateSampler(
        vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat, m_Properties.limits.maxSamplerAnisotropy, false, vk::CompareOp::eNever,
        vk::BorderColor::eFloatOpaqueWhite, vk::SamplerReductionMode::eWeightedAverage,
        "DefaultSampler"
    );

    uint8_t whitePixel[4] = { 255, 0, 255, 255 };

    vk::ImageCreateInfo imageInfo(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format::eR8G8B8A8Unorm,
        vk::Extent3D(1, 1, 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    );

    m_DefaultTexture = lnnew Texture(this, imageInfo, "DefaultTexture");
    m_DefaultTexture->UploadData(whitePixel);
}

void GfxContext::NukeDefaultResources()
{
    delete m_DefaultTexture;
    m_Device.destroySampler(m_DefaultSampler);
}

void GfxContext::WaitIdle() const
{
    m_Device.waitIdle();
}

vkb::PhysicalDevice GfxContext::VkbSelectPhysicalDevice(const vkb::Instance& instance, vk::SurfaceKHR surface)
{
    auto physDeviceSelect = vkb::PhysicalDeviceSelector(instance);

    auto deviceFeatures = VkPhysicalDeviceFeatures{
        .imageCubeArray = vk::True,
        .geometryShader = vk::True, // for im3d
        .depthClamp = vk::True,
        .samplerAnisotropy = vk::True,
    };

    auto features12 = VkPhysicalDeviceVulkan12Features{
        .descriptorIndexing = vk::True,
        .shaderSampledImageArrayNonUniformIndexing = vk::True,
        .descriptorBindingSampledImageUpdateAfterBind = vk::True,
        .descriptorBindingStorageImageUpdateAfterBind = vk::True,
        .descriptorBindingPartiallyBound = vk::True,
        .runtimeDescriptorArray = vk::True,
        .scalarBlockLayout = vk::True,
    };

    auto features13 = VkPhysicalDeviceVulkan13Features{
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    physDeviceSelect.set_surface(surface)
        .set_minimum_version(1, 3)
        .set_required_features(deviceFeatures)
        .set_required_features_12(features12)
        .set_required_features_13(features13)
        .add_required_extension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    vkb::Result<vkb::PhysicalDevice> selectedDevice = physDeviceSelect.select();

    LNE_ASSERT(selectedDevice, "Failed to select a physical device");

    return selectedDevice.value();
}

vk::SurfaceCapabilitiesKHR GfxContext::GetSurfaceCapabilities(vk::SurfaceKHR surface) const
{
    return m_PhysicalDevice.getSurfaceCapabilitiesKHR(surface);
}

std::vector<vk::SurfaceFormatKHR> GfxContext::GetSurfaceFormats(vk::SurfaceKHR surface) const
{
    return m_PhysicalDevice.getSurfaceFormatsKHR(surface);
}

std::vector<vk::PresentModeKHR> GfxContext::GetSurfacePresentModes(vk::SurfaceKHR surface) const
{
    return m_PhysicalDevice.getSurfacePresentModesKHR(surface);
}

std::string GfxContext::GetQueueFamilyName(EQueueFamilyType type) const
{
    switch (type)
    {
    case EQueueFamilyType::Graphics:
        return "Graphics";
    case EQueueFamilyType::Compute:
        return "Compute";
    case EQueueFamilyType::Transfer:
        return "Transfer";
    case EQueueFamilyType::Present:
        return "Present";
    }
    return std::string();
}

uint32_t GfxContext::GetQueueFamilyIndex(EQueueFamilyType type) const
{
    switch (type)
    {
    case EQueueFamilyType::Graphics:
        return m_QueueFamilyIndices.GraphicsFamily.value();
    case EQueueFamilyType::Compute:
        return m_QueueFamilyIndices.ComputeFamily.value();
    case EQueueFamilyType::Transfer:
        return m_QueueFamilyIndices.TransferFamily.value();
    case EQueueFamilyType::Present:
        return m_QueueFamilyIndices.PresentFamily.value();
    }
    LNE_ERROR("Invalid queue family type");
    throw std::runtime_error("Invalid queue family type");
}

vk::Queue GfxContext::GetQueue(EQueueFamilyType type) const
{
    switch (type)
    {
    case EQueueFamilyType::Graphics:
        return m_GraphicsQueue;
    case EQueueFamilyType::Compute:
        return m_ComputeQueue;
    case EQueueFamilyType::Transfer:
        return m_TransferQueue;
    case EQueueFamilyType::Present:
        return m_PresentQueue;
    }
    LNE_ERROR("Invalid queue family type");
    throw std::runtime_error("Invalid queue family type");
}

vk::CommandPool GfxContext::CreateCommandPool(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags) const
{
    vk::CommandPoolCreateInfo poolInfo(flags, queueFamilyIndex);
    auto cp = m_Device.createCommandPool(poolInfo);
    SetVkObjectName(cp, std::format("CommandPool {}", queueFamilyIndex));
    return cp;
}

vk::ImageView GfxContext::CreateImageView(vk::Image image, vk::ImageViewType viewType, vk::Format format, uint32_t numMipLevels, uint32_t layers, vk::ImageAspectFlags aspectMask, const std::string& name)
{
    vk::ImageViewCreateInfo createInfo(
        {},
        image,
        viewType,
        format
    );
    createInfo.subresourceRange = vk::ImageSubresourceRange(
        aspectMask,
        0,
        numMipLevels,
        0,
        layers
    );
    auto imageView = m_Device.createImageView(createInfo);
    SetVkObjectName(imageView, std::format("ImageView: {}", name));
    return imageView;
}

uint32_t GfxContext::RegisterBindlessTexture(Texture* texture)
{
    vk::Sampler sampler = texture->GetSampler();
    if (sampler == nullptr)
        sampler = m_DefaultSampler;

    uint32_t textureIndex = m_FreeBindlessIndices.front();
    auto imageInfo = vk::DescriptorImageInfo{
        sampler,
        texture->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    };
    vk::WriteDescriptorSet descriptorWrite{
        m_BindlessDescriptorSet,
        0,
        textureIndex,
        vk::DescriptorType::eCombinedImageSampler,
        imageInfo
    };
    m_Device.updateDescriptorSets({ descriptorWrite }, nullptr);
    m_FreeBindlessIndices.pop();
    return textureIndex;
}

void GfxContext::FreeBindlessImage(uint32_t index)
{
    m_FreeBindlessIndices.push(index);
}

vk::Sampler GfxContext::CreateSampler(vk::Filter magFilter, vk::Filter minFilter, vk::SamplerMipmapMode mipmapMode, 
    vk::SamplerAddressMode addressMode, float maxAnisotropy, bool compareEnable, vk::CompareOp compareOp, vk::BorderColor borderColor, 
    vk::SamplerReductionMode reductionMode, const std::string& name)
{
    vk::SamplerCreateInfo samplerInfo = vk::SamplerCreateInfo{
        {},
        magFilter,
        minFilter,
        mipmapMode,
        addressMode,
        addressMode,
        addressMode,
        0,
        maxAnisotropy > 0.0f,
        maxAnisotropy,
        compareEnable,
        compareOp,
        0,
        16,
        borderColor,
        vk::False
    };

    auto sampler = m_Device.createSampler(samplerInfo);
    SetVkObjectName(sampler, std::format("Sampler: {}", name));
    return sampler;
}

vk::DescriptorSetLayout GfxContext::CreateDescriptorSetLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings, const std::string& name)
{
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
    auto layout = m_Device.createDescriptorSetLayout(layoutInfo);
    SetVkObjectName(layout, std::format("DescriptorSetLayout: {}", name));
    return layout;
}

void GfxContext::AllocateBuffer(BufferAllocation& allocation, VkBufferCreateInfo bufferCI, VmaAllocationCreateInfo allocCI)
{
    VkBuffer buffer;
    VK_CHECK_C(vmaCreateBuffer(m_MemoryAllocator,
        &bufferCI, &allocCI, 
        &buffer, &allocation.Allocation, &allocation.AllocationInfo));
    allocation.Buffer = buffer;

    VkMemoryPropertyFlags memPropFlags;
    vmaGetAllocationMemoryProperties(m_MemoryAllocator, allocation.Allocation, &memPropFlags);
    allocation.MemoryFlags = vk::MemoryPropertyFlags(memPropFlags);
}

BufferAllocation GfxContext::AllocateStagingBuffer(uint64_t size)
{
    vk::BufferCreateInfo stagingBufferCI{
        {},
        size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo stagingAllocCI{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    BufferAllocation stagingAllocation;

    AllocateBuffer(stagingAllocation, stagingBufferCI, stagingAllocCI);

    return stagingAllocation;
}

void GfxContext::FreeBuffer(BufferAllocation& allocation)
{
    vmaDestroyBuffer(m_MemoryAllocator, allocation.Buffer, allocation.Allocation);
}

void GfxContext::AllocateImage(ImageAllocation& allocation, VkImageCreateInfo imageCI, VmaAllocationCreateInfo allocCI)
{
    VkImage image;
    VK_CHECK_C(vmaCreateImage(m_MemoryAllocator,
        &imageCI, &allocCI,
        &image, &allocation.Allocation, &allocation.AllocationInfo));
    allocation.Image = image;
}

void GfxContext::FreeImage(ImageAllocation & allocation)
{
    vmaDestroyImage(m_MemoryAllocator, allocation.Image, allocation.Allocation);
}

SafePtr<Shader> GfxContext::CreateShader(std::string_view filePath)
{
    SafePtr<Shader> shader;
    shader.Reset(lnnew Shader(SafePtr(this), filePath));
    return shader;
}

VkBool32 VKAPI_CALL GfxContext::DebugPrintfCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        LNE_TRACE("Validation layer: {0}", pCallbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LNE_INFO("Validation layer: {0}", pCallbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LNE_WARN("Validation layer: {0}", pCallbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LNE_ERROR("Validation layer: {0}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

void GfxContext::CreateMemoryAllocator()
{
    VmaVulkanFunctions allocatorFunctions = {
        .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = VULKAN_HPP_DEFAULT_DISPATCHER.vkAllocateMemory,
        .vkFreeMemory = VULKAN_HPP_DEFAULT_DISPATCHER.vkFreeMemory,
        .vkMapMemory = VULKAN_HPP_DEFAULT_DISPATCHER.vkMapMemory,
        .vkUnmapMemory = VULKAN_HPP_DEFAULT_DISPATCHER.vkUnmapMemory,
        .vkFlushMappedMemoryRanges = VULKAN_HPP_DEFAULT_DISPATCHER.vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = VULKAN_HPP_DEFAULT_DISPATCHER.vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = VULKAN_HPP_DEFAULT_DISPATCHER.vkBindBufferMemory,
        .vkBindImageMemory = VULKAN_HPP_DEFAULT_DISPATCHER.vkBindImageMemory,
        .vkGetBufferMemoryRequirements = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetImageMemoryRequirements,
        .vkCreateBuffer = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateBuffer,
        .vkDestroyBuffer = VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyBuffer,
        .vkCreateImage = VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImage,
        .vkDestroyImage = VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyImage,
        .vkCmdCopyBuffer = VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = VULKAN_HPP_DEFAULT_DISPATCHER.vkBindBufferMemory2,
        .vkBindImageMemory2KHR = VULKAN_HPP_DEFAULT_DISPATCHER.vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceMemoryProperties2KHR,
        .vkGetDeviceBufferMemoryRequirements = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceImageMemoryRequirements,
    };

    VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = m_PhysicalDevice,
        .device = m_Device,
        .pVulkanFunctions = &allocatorFunctions,
        .instance = s_VulkanInstance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    vmaCreateAllocator(&allocatorInfo, &m_MemoryAllocator);

    LNE_ASSERT(m_MemoryAllocator, "Failed to create memory allocator");
}

void GfxContext::DumpMemoryStats(std::string_view fileName) const
{
    char* memoryStats{ nullptr };
    LNE_ASSERT(m_MemoryAllocator, "Allocator must be initialized");
    vmaBuildStatsString(m_MemoryAllocator, &memoryStats, true);

    LNE_INFO("Memory stats: {0}", std::string_view(memoryStats));

    vmaFreeStatsString(m_MemoryAllocator, memoryStats);
}
}
