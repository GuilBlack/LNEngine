#pragma once
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include "PhysicalDevice.h"
#include "Device.h"

namespace lne
{
class GfxContext
{
public:
    GfxContext(vk::SurfaceKHR surface);
    virtual ~GfxContext();

    static bool InitVulkan(std::string appName);
    static void NukeVulkan();

    static [[nodiscard]] vk::Instance GetVulkanInstance() { return s_VulkanInstance; }

private:
    static class vk::Instance s_VulkanInstance;
    static vkb::Instance s_VkbInstance;
    static bool s_DynamicLoaderInitialized;

private:
    std::unique_ptr<class PhysicalDevice> m_PhysicalDevice;
    std::unique_ptr<class Device> m_Device;
    VmaAllocator m_MemoryAllocator;

private:
    static VkBool32 VKAPI_CALL DebugPrintfCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

private:
    void CreateMemoryAllocator();
    void DumpMemoryStats(std::string_view fileName) const;
};
}
