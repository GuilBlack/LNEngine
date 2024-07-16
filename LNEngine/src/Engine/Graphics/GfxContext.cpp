#include "GfxContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "VulkanUtils.h"
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

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
    auto vkbPhysicalDevice = PhysicalDevice::VkbSelectPhysicalDevice(s_VkbInstance, surface);
    m_PhysicalDevice.reset(new PhysicalDevice(vkbPhysicalDevice));
    m_Device.reset(new Device(vkbPhysicalDevice, surface));
    CreateMemoryAllocator();
}

GfxContext::~GfxContext()
{
    vmaDestroyAllocator(m_MemoryAllocator);
    m_Device.reset();
    m_PhysicalDevice.reset();
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
    std::vector<std::string> instanceLayers = vkut::filterExtensions(requiredLayers, availableLayers);

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
    std::vector<std::string> instanceExtensions = vkut::filterExtensions(requiredExtensions,
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
        .physicalDevice = m_PhysicalDevice->GetHandle(),
        .device = m_Device->GetHandle(),
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
