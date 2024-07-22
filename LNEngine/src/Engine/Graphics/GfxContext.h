#pragma once
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include "PhysicalDevice.h"
#include "VulkanUtils.h"
#include "Device.h"
#include "Swapchain.h"

namespace lne
{
class GfxContext
{
public:
    GfxContext(vk::SurfaceKHR surface);
    ~GfxContext();

    static bool InitVulkan(std::string appName);
    static void NukeVulkan();

    static vk::Instance VulkanInstance() { return s_VulkanInstance; }
    class std::shared_ptr<PhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
    class std::shared_ptr<Device> GetDevice() const { return m_Device; }
    const VmaAllocator& GetMemoryAllocator() const { return m_MemoryAllocator; }

    template<typename T>
    void SetVkObjectName(T handle, vk::ObjectType type, const std::string& name) const
    {
    #if defined(VK_EXT_debug_utils)
        const vk::DebugUtilsObjectNameInfoEXT objectNameInfo(
            type,
            reinterpret_cast<uint64_t>(static_cast<T::CType>(handle)),
            name.c_str()
        );
        VK_CHECK(m_Device->GetHandle().setDebugUtilsObjectNameEXT(&objectNameInfo));
    #else
        (void)handle;
        (void)type;
        (void)name;
    #endif
    }

private:
    static class vk::Instance s_VulkanInstance;
    static vkb::Instance s_VkbInstance;
    static bool s_DynamicLoaderInitialized;

private:
    std::shared_ptr<class PhysicalDevice> m_PhysicalDevice;
    std::shared_ptr<class Device> m_Device;
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
