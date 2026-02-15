#pragma once
#include "../vendor/VKBOOTSTRAP/vkbootstrap/src/VkBootstrap.h"
#include "../vendor/VMA/vk_mem_alloc.h"

#include "VulkanUtils.h"
#include "Swapchain.h"
#include "Enums.h"
#include "Structs.h"
#include "Engine/Graphics/Resources/Shader.h"
#include "Engine/Core/SafePtr.h"
#include "DynamicDescriptorAllocator.h"
#include "Engine/Core/Utils/Log.h"
#include "Engine/Core/Utils/_Defines.h"

namespace lne
{
class Texture;

struct QueueFamilyIndices
{
    std::optional<u32> GraphicsFamily;
    std::optional<u32> ComputeFamily;
    std::optional<u32> TransferFamily;
    std::optional<u32> PresentFamily;

    bool                                    IsComplete() const
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
    static constexpr u32   s_MaxSSBOsPerSet = 15;

public:
    GfxContext(vk::SurfaceKHR surface);
    virtual ~GfxContext();

    static bool                             InitVulkan(std::string appName);
    static void                             NukeVulkan();

    void                                    InitDefaultResources();
    void                                    UploadDefaultResources();
    void                                    NukeDefaultResources();
    void                                    DeferredNukeResources();

    void                                    EnqueueResourceDeletion(const ResourceDeletion& deletion)
    {
        std::lock_guard<std::mutex> lock(m_ResourceDeletionMutex);
        m_ResourceDeletionQueue.push_back(deletion);
    }

    void                                    WaitIdle() const;

    static vk::Instance                     VulkanInstance() { return s_VulkanInstance; }
    vk::PhysicalDevice                      GetPhysicalDevice() const { return m_PhysicalDevice; }
    vk::Device                              GetDevice() const { return m_Device; }
    // Gets the current frame in flight on the main thread
    [[nodiscard]] constexpr u32             GetCurrentFrameIndex() const { return m_CurrentFrameInFlight; }
    [[nodiscard]] constexpr u32             GetMaxFramesInFlight() const { return m_MaxFramesInFlight; }
    [[nodiscard]] VmaAllocator              GetMemoryAllocator() const { return m_MemoryAllocator; }
    [[nodiscard]] class CommandPoolManager& GetCommandPoolManager() const
    { return *m_CommandPoolManager; };
    [[nodiscard]] const class Geometry&     GetDefaultFullscreenQuad() const { return *m_DefaultFullscreenQuad; }
    [[nodiscard]] SafePtr<Texture>          GetDefaultTexture() const;
    [[nodiscard]] SafePtr<Texture>          GetWhiteTexture() const;
    [[nodiscard]] vk::Sampler               GetDefaultSampler() const { return m_DefaultSampler; }
    [[nodiscard]] vk::Sampler               GetDepthSampler() const { return m_DepthSampler; }

    // numBindings MUST be in range [1, s_MaxSSBOsPerSet]
    [[nodiscard]] vk::DescriptorSetLayout   GetStorageOnlyDescriptorSetLayout(u32 numBindings) const
    { 
        LNE_ASSERT(numBindings >= 1 && numBindings <= s_MaxSSBOsPerSet, std::format("numBindings must be in range [1, {0}]", s_MaxSSBOsPerSet));
        return m_StorageOnlyDescriptorSetLayouts[numBindings - 1];
    }

#pragma region PhysicalDevice
    [[nodiscard]] const vk::PhysicalDeviceProperties&           GetProperties() const { return m_Properties; }
    [[nodiscard]] const vk::PhysicalDeviceFeatures&             GetEnabledFeatures() const { return m_EnabledFeatures; }
    [[nodiscard]] const vk::PhysicalDeviceMemoryProperties&     GetMemoryProperties() const { return m_MemoryProperties; }
    [[nodiscard]] const std::vector<vk::QueueFamilyProperties>& GetQueueFamilyProperties() const { return m_QueueFamilyProperties; }

    [[nodiscard]] vk::SurfaceCapabilitiesKHR                    GetSurfaceCapabilities(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::SurfaceFormatKHR>             GetSurfaceFormats(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::PresentModeKHR>               GetSurfacePresentModes(vk::SurfaceKHR surface) const;
#pragma endregion

#pragma region Queues
    [[nodiscard]] const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
    [[nodiscard]] std::string               GetQueueFamilyName(EQueueFamilyType type) const;
    [[nodiscard]] u32                       GetQueueFamilyIndex(EQueueFamilyType type) const;
    [[nodiscard]] vk::Queue                 GetQueue(EQueueFamilyType type) const;

    void                                    SubmitToQueue(EQueueFamilyType type,
                                                          const vk::SubmitInfo& submitInfo,
                                                          vk::Fence fence);
#pragma endregion

#pragma region CommandBuffers

    [[nodiscard]] vk::CommandPool           CreateCommandPool(u32 queueFamilyIndex,
                                                              vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const;

    [[nodiscard]] vk::CommandBuffer         GetPrimaryCommandBuffer() const;
#pragma endregion

#pragma region Images

    [[nodiscard]] vk::ImageView             CreateImageView(vk::Image image, vk::ImageViewType viewType,
                                                            vk::Format format, u32 numMipLevels = 1,
                                                            u32 layers = 1, 
                                                            vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, 
                                                            const std::string& name = "");

    [[nodiscard]] BindlessImageHandle       RegisterBindlessTexture(class Texture* texture);
    [[nodiscard]] BindlessImageHandle       RegisterBindlessImage(vk::ImageView imageView);

    [[nodiscard]] vk::Sampler               CreateSampler(vk::Filter magFilter = vk::Filter::eLinear,
                                                          vk::Filter minFilter = vk::Filter::eLinear,
                                                          vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear,
                                                          vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat,
                                                          float maxAnisotropy = 1.0f,
                                                          bool compareEnable = false,
                                                          vk::CompareOp compareOp = vk::CompareOp::eAlways,
                                                          float minLod = 0.f, float maxLod = 0.f, float mipLodBias = 0.f,
                                                          vk::BorderColor borderColor = vk::BorderColor::eFloatOpaqueWhite,
                                                          vk::SamplerReductionMode reductionMode = vk::SamplerReductionMode::eWeightedAverage,
                                                          const std::string& name = "");

    [[nodiscard]] vk::DescriptorSetLayout   GetBindlessDescriptorSetLayout() const { return m_BindlessDescriptorSetLayout; }
    [[nodiscard]] vk::DescriptorSet         GetBindlessDescriptorSet() const { return m_BindlessDescriptorSet; }

#pragma endregion

#pragma region Allocations

    void                                    AllocateBuffer(BufferAllocation& allocation, VkBufferCreateInfo bufferCI, VmaAllocationCreateInfo allocCI);
    BufferAllocation                        AllocateStagingBuffer(u64 size);
    void                                    FreeBufferAllocation(const BufferAllocation& allocation);

    void                                    AllocateImage(ImageAllocation& allocation, VkImageCreateInfo imageCI, VmaAllocationCreateInfo allocCI);
    void                                    FreeImageAllocation(const ImageAllocation& allocation);

    [[nodiscard]] vk::DescriptorSet         AllocateDescriptorSet(vk::DescriptorSetLayout layout, DescriptorType::Enum descriptorType);
    void                                    FreeDescriptorSet(vk::DescriptorSet descriptorSet, DescriptorType:: Enum descriptorType);

#pragma endregion

#pragma region Shader

    [[nodiscard]] SafePtr<Shader>           CreateShader(std::string_view filePath);
    [[nodiscard]] vk::DescriptorSetLayout   CreateDescriptorSetLayout(const std::vector<vk::DescriptorSetLayoutBinding>&bindings, const std::string & name = "");

#pragma endregion

#pragma region Utils
    template<typename T>
    void                                    SetVkObjectName(T handle, std::string_view name) const
    {
    #if defined(VK_EXT_debug_utils) && defined(LNE_DEBUG)
        const vk::DebugUtilsObjectNameInfoEXT objectNameInfo(
            T::objectType,
            reinterpret_cast<u64>(static_cast<T::CType>(handle)),
            name.data()
        );
        VK_CHECK(m_Device.setDebugUtilsObjectNameEXT(&objectNameInfo));
    #else
        (void)handle;
        (void)name;
    #endif
    }

#pragma endregion

private:
    static class vk::Instance               s_VulkanInstance;
    static vkb::Instance                    s_VkbInstance;
    static bool                             s_DynamicLoaderInitialized;
    using SSBODescriptorSetLayoutArray = std::array<vk::DescriptorSetLayout, s_MaxSSBOsPerSet>;

    vk::PhysicalDevice                      m_PhysicalDevice;
    vk::Device                              m_Device;
    VmaAllocator                            m_MemoryAllocator;
    vk::PhysicalDeviceProperties            m_Properties;
    vk::PhysicalDeviceFeatures              m_EnabledFeatures;
    vk::PhysicalDeviceMemoryProperties      m_MemoryProperties;
    std::vector<vk::QueueFamilyProperties>  m_QueueFamilyProperties;

    QueueFamilyIndices                      m_QueueFamilyIndices;
    vk::Queue                               m_GraphicsQueue;
    vk::Queue                               m_ComputeQueue;
    vk::Queue                               m_TransferQueue;
    vk::Queue                               m_PresentQueue;

    std::mutex                              m_QueueMutex;

    u32                                     m_CurrentFrameInFlight{ 0 };
    u32                                     m_MaxFramesInFlight{ 2 };

    std::unique_ptr<class CommandPoolManager> m_CommandPoolManager;

    vk::Sampler                             m_DefaultSampler;
    vk::Sampler                             m_DepthSampler;
    SafePtr<Texture>                        m_DefaultTexture;
    SafePtr<Texture>                        m_WhitePixel;
    class Geometry*                         m_DefaultFullscreenQuad;

    vk::DescriptorPool                      m_BindlessDescriptorPool;
    vk::DescriptorSetLayout                 m_BindlessDescriptorSetLayout;
    vk::DescriptorSet                       m_BindlessDescriptorSet;
    std::queue<BindlessImageHandle>         m_FreeBindlessTextureIndices{};
    std::queue<BindlessImageHandle>         m_FreeBindlessImageIndices{};
    std::mutex                              m_BindlessMutex{};

    std::unique_ptr<DynamicDescriptorAllocator> m_UniformOnlyDescriptorAllocator{};
    std::unique_ptr<DynamicDescriptorAllocator> m_StorageOnlyDescriptorAllocator{};
    std::unique_ptr<DynamicDescriptorAllocator> m_UniformStorageDescriptorAllocator{};
    SSBODescriptorSetLayoutArray            m_StorageOnlyDescriptorSetLayouts{};

    std::mutex                              m_ResourceDeletionMutex{};
    std::vector<ResourceDeletion>           m_ResourceDeletionQueue{};

    friend class Swapchain;
    friend class Renderer;

private:
    static VkBool32 VKAPI_CALL              DebugPrintfCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                                VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                                void* pUserData);

private:
    vkb::PhysicalDevice                     VkbSelectPhysicalDevice(const vkb::Instance& instance, vk::SurfaceKHR surface);

    void                                    NukeResource(const ResourceDeletion& resource);
    void                                    NukeBuffer(const BufferResourceDeletion& buffer);
    void                                    NukeImage(const TextureResourceDeletion& image);
    void                                    NukePipeline(const PipelineResourceDeletion& pipeline);
    void                                    NukeShader(const ShaderResourceDeletion& shader);
    void                                    NukeImageView(const ImageViewDeletion& imageView);

    void                                    CreateMemoryAllocator();
    void                                    DumpMemoryStats(std::string_view fileName) const;
};
}
