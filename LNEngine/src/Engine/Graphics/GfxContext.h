#pragma once
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include "VulkanUtils.h"
#include "Swapchain.h"

namespace lne
{
struct QueueFamilyIndices
{
    std::optional<uint32_t> GraphicsFamily;
    std::optional<uint32_t> ComputeFamily;
    std::optional<uint32_t> TransferFamily;
    std::optional<uint32_t> PresentFamily;

    [[nodiscard]] bool IsComplete() const
    {
        return GraphicsFamily.has_value()
            && ComputeFamily.has_value()
            && TransferFamily.has_value()
            && PresentFamily.has_value();
    }
};

class GfxContext
{
public:
    GfxContext(vk::SurfaceKHR surface);
    ~GfxContext();

    static bool InitVulkan(std::string appName);
    static void NukeVulkan();

    static vk::Instance VulkanInstance() { return s_VulkanInstance; }
    class vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    class vk::Device GetDevice() const { return m_Device; }
    const VmaAllocator& GetMemoryAllocator() const { return m_MemoryAllocator; }

#pragma region PhysicalDevice
    [[nodiscard]] const vk::PhysicalDeviceProperties& GetProperties() const { return m_Properties; }
    [[nodiscard]] const vk::PhysicalDeviceFeatures& GetEnabledFeatures() const { return m_EnabledFeatures; }
    [[nodiscard]] const vk::PhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_MemoryProperties; }
    [[nodiscard]] const std::vector<vk::QueueFamilyProperties>& GetQueueFamilyProperties() const { return m_QueueFamilyProperties; }

    [[nodiscard]] vk::SurfaceCapabilitiesKHR GetSurfaceCapabilities(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::SurfaceFormatKHR> GetSurfaceFormats(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::PresentModeKHR> GetSurfacePresentModes(vk::SurfaceKHR surface) const;
#pragma endregion

#pragma region Queues
    [[nodiscard]] vk::Queue GetGraphicsQueue() const { return m_GraphicsQueue; }
    [[nodiscard]] vk::Queue GetComputeQueue() const { return m_ComputeQueue; }
    [[nodiscard]] vk::Queue GetTransferQueue() const { return m_TransferQueue; }
    [[nodiscard]] vk::Queue GetPresentQueue() const { return m_PresentQueue; }

    [[nodiscard]] const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
#pragma endregion

    template<typename T>
    void SetVkObjectName(T handle, vk::ObjectType type, const std::string& name) const
    {
    #if defined(VK_EXT_debug_utils)
        const vk::DebugUtilsObjectNameInfoEXT objectNameInfo(
            type,
            reinterpret_cast<uint64_t>(static_cast<T::CType>(handle)),
            name.c_str()
        );
        VK_CHECK(m_Device.setDebugUtilsObjectNameEXT(&objectNameInfo));
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
    class vk::PhysicalDevice m_PhysicalDevice;
    class vk::Device m_Device;
    VmaAllocator m_MemoryAllocator;
    vk::PhysicalDeviceProperties m_Properties;
    vk::PhysicalDeviceFeatures m_EnabledFeatures;
    vk::PhysicalDeviceMemoryProperties m_MemoryProperties;
    std::vector<vk::QueueFamilyProperties> m_QueueFamilyProperties;

    QueueFamilyIndices m_QueueFamilyIndices;

    vk::Queue m_GraphicsQueue;
    vk::Queue m_ComputeQueue;
    vk::Queue m_TransferQueue;
    vk::Queue m_PresentQueue;

private:
    static VkBool32 VKAPI_CALL DebugPrintfCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

private:
    vkb::PhysicalDevice VkbSelectPhysicalDevice(const vkb::Instance& instance, vk::SurfaceKHR surface);

    void CreateMemoryAllocator();
    void DumpMemoryStats(std::string_view fileName) const;
};
}
