#pragma once
#include "Engine/Graphics/Enums.h"
#include "Engine/Graphics/Structs.h"

namespace lne
{
class GfxContext;
class Texture;

class CommandBuffer
{
public:
    enum Type
    {
        ePrimary,
        eSecondary
    };
public:
    CommandBuffer(GfxContext* contextRef, vk::CommandPool commandPoolRef,
                  EQueueFamilyType queueType, Type type,
                  std::string_view name = "");

    vk::CommandBuffer           GetVkCommandBuffer() const { return m_CommandBuffer; }

#pragma region CommandBuffer Operations
    void                        ClearState();
    void                        Reset() { m_CommandBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources); }

    void                        BeginRecording(vk::CommandBufferUsageFlags usage);
    void                        BeginRecording(vk::CommandBufferUsageFlags usage,
                                               const vk::CommandBufferInheritanceInfo& inheritanceInfo);
    void                        BeginRecording(vk::CommandBufferUsageFlags usage,
                                               const vk::CommandBufferInheritanceInfo& inheritanceInfo,
                                               const vk::CommandBufferInheritanceRenderingInfo& renderInheritanceInfo);
    void                        EndRecording();
#pragma endregion

#pragma region Image Operations
    void                        TransitionLayout(Texture* textureRef, vk::ImageLayout newLayout,
                                                 u32 srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                 u32 dstQueueFamily = VK_QUEUE_FAMILY_IGNORED);
    void                        TransitionLayoutMips(Texture* textureRef,
                                                     vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                                     u32 baseMip, u32 mipLevels,
                                                     u32 srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                     u32 dstQueueFamily = VK_QUEUE_FAMILY_IGNORED);
    void                        TransitionLayout(Texture* textureRef,
                                                 vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                                 u32 baseMip, u32 mipLevels, u32 baseLayer, u32 numLayers,
                                                 u32 srcQueueFamily, u32 dstQueueFamily,
                                                 bool changeTextureLayout);

    void                        CopyBufferToImage(const BufferAllocation& bufferAlloc, Texture* textureRef,
                                                  vk::ImageLayout dstLayout, std::span<vk::BufferImageCopy> regions);

    void                        GenerateMips(Texture* textureRef);
    void                        Blit(Texture* srcRef, Texture* dstRef);
#pragma endregion

#pragma region Buffer Operations
    void                        CopyBuffer(const BufferAllocation& srcBuffer, const BufferAllocation& dstBuffer,
                                           std::span<vk::BufferCopy> regions);
#pragma endregion

#pragma region Frame Operations
    void                        SetViewport(const vk::Viewport& viewport);
    void                        SetScissor(const vk::Rect2D& scissor);
#pragma endregion


#pragma region Utilities
    void                        PushLabel(std::string_view label, const glm::vec4& color = glm::vec4(1.0f));
    void                        PopLabel();
#pragma endregion

private:
    Type                        m_Type;
    EQueueFamilyType            m_QueueType;
    bool                        m_IsRecording{ false };

    vk::CommandPool             m_CommandPoolRef;
    vk::CommandBuffer           m_CommandBuffer;

private:
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) = delete;
    CommandBuffer& operator=(CommandBuffer&&) = delete;
};
}
