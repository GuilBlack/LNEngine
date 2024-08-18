#pragma once
#include "../vendor/VKBOOTSTRAP/vkbootstrap/src/VkBootstrap.h"
#include "../vendor/VMA/vk_mem_alloc.h"

#include "VulkanUtils.h"
#include "Swapchain.h"
#include "GfxEnums.h"
#include "Enums.h"
#include "Shader.h"
#include "Engine/Core/SafePtr.h"

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

class GfxContext : public RefCountBase
{
public:
    GfxContext(vk::SurfaceKHR surface);
    virtual ~GfxContext();

    static bool InitVulkan(std::string appName);
    static void NukeVulkan();

    void WaitIdle() const;

    static vk::Instance VulkanInstance() { return s_VulkanInstance; }
    class vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    class vk::Device GetDevice() const { return m_Device; }
    [[nodiscard]] constexpr uint32_t GetCurrentFrameIndex() const { return m_CurrentFrameInFlight; }
    [[nodiscard]] constexpr uint32_t GetMaxFramesInFlight() const { return m_MaxFramesInFlight; }
    [[nodiscard]] VmaAllocator GetMemoryAllocator() const { return m_MemoryAllocator; }
    [[nodiscard]] class CommandBufferManager& GetTransferCommandBufferManager() const { return *m_TransferCommandBufferManager; }

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
    [[nodiscard]] const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
    [[nodiscard]] std::string GetQueueFamilyName(EQueueFamilyType type) const;
    [[nodiscard]] uint32_t GetQueueFamilyIndex(EQueueFamilyType type) const;
    [[nodiscard]] vk::Queue GetQueue(EQueueFamilyType type) const;
#pragma endregion

#pragma region CommandBuffers

    [[nodiscard]] vk::CommandPool CreateCommandPool(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const;

#pragma endregion

#pragma region Image

    [[nodiscard]] vk::ImageView CreateImageView(vk::Image image, vk::ImageViewType viewType,
        vk::Format format, uint32_t numMipLevels = 1,
        uint32_t layers = 1, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, const std::string& name = "");

#pragma endregion

    vk::DescriptorSetLayout CreateDescriptorSetLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings, const std::string& name = "");

#pragma region Buffer

    void AllocateBuffer(BufferAllocation& allocation, VkBufferCreateInfo bufferCI, VmaAllocationCreateInfo allocCI);
    void FreeBuffer(BufferAllocation& allocation);

#pragma endregion

#pragma region Shader

    [[nodiscard]] SafePtr<Shader> CreateShader(std::string_view filePath);

#pragma endregion

    template<typename T>
    void SetVkObjectName(T handle, std::string_view name) const
    {
    #if defined(VK_EXT_debug_utils)
        const vk::DebugUtilsObjectNameInfoEXT objectNameInfo(
            T::objectType,
            reinterpret_cast<uint64_t>(static_cast<T::CType>(handle)),
            name.data()
        );
        VK_CHECK(m_Device.setDebugUtilsObjectNameEXT(&objectNameInfo));
    #else
        (void)handle;
        (void)name;
    #endif
    }

private:
    static class vk::Instance s_VulkanInstance;
    static vkb::Instance s_VkbInstance;
    static bool s_DynamicLoaderInitialized;

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

    uint32_t m_CurrentFrameInFlight{ 0 };
    uint32_t m_MaxFramesInFlight{ 2 };

    std::unique_ptr<class CommandBufferManager> m_TransferCommandBufferManager;

    friend class Swapchain;

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
