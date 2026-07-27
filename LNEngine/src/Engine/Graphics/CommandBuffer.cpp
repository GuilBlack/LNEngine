#include "CommandBuffer.h"

#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Profiling.h"

#include "Graphics/VulkanUtils.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Resources/Texture.h"
#include "Graphics/Resources/StorageBuffer.h"

namespace lne
{
CommandBuffer::CommandBuffer(GfxContext* contextRef, vk::CommandPool commandPoolRef,
                             EQueueFamilyType queueType, Type type,
                             std::string_view name)
    : m_CommandPoolRef(commandPoolRef), m_QueueType(queueType), m_Type(type)
{
    vk::CommandBufferLevel level;
    switch (type)
    {
    case ePrimary:
        level = vk::CommandBufferLevel::ePrimary;
        break;
    case eSecondary:
        level = vk::CommandBufferLevel::eSecondary;
        break;
    default:
        LNE_ASSERT(false, "Invalid command buffer type.");
        level = vk::CommandBufferLevel::ePrimary;
        break;
    }
    vk::CommandBufferAllocateInfo allocInfo(m_CommandPoolRef, level, 1);
    VK_CHECK(contextRef->GetDevice().allocateCommandBuffers(&allocInfo, &m_CommandBuffer));
    contextRef->SetVkObjectName(m_CommandBuffer, name);
}

#pragma region CommandBuffer Operations
void CommandBuffer::ClearState()
{

}

void CommandBuffer::BeginRecording(vk::CommandBufferUsageFlags usage)
{
    m_CommandBuffer.begin(vk::CommandBufferBeginInfo(usage));
    m_IsRecording = true;
}

void CommandBuffer::BeginRecording(vk::CommandBufferUsageFlags usage, const vk::CommandBufferInheritanceInfo& inheritanceInfo)
{
    LNE_ASSERT(m_Type == eSecondary, "Inheritance info can only be provided for secondary command buffers.");
    m_CommandBuffer.begin(vk::CommandBufferBeginInfo(usage, &inheritanceInfo));
    m_IsRecording = true;
}

void CommandBuffer::BeginRecording(vk::CommandBufferUsageFlags usage,
                                   const vk::CommandBufferInheritanceInfo& inheritanceInfo,
                                   const vk::CommandBufferInheritanceRenderingInfo& renderInheritanceInfo)
{
    LNE_ASSERT(m_Type == eSecondary, "RenderPassContinue usage flag can only be set for secondary command buffers.");
    LNE_ASSERT((bool)(usage & vk::CommandBufferUsageFlagBits::eRenderPassContinue), "RenderPassContinue usage flag must be set when providing render inheritance info.");
    vk::StructureChain<
        vk::CommandBufferInheritanceInfo,
        vk::CommandBufferInheritanceRenderingInfo> chain{
            inheritanceInfo,
            renderInheritanceInfo
    };
    m_CommandBuffer.begin(vk::CommandBufferBeginInfo(usage, &chain.get<vk::CommandBufferInheritanceInfo>()));
    m_IsRecording = true;
}

void CommandBuffer::EndRecording()
{
    m_CommandBuffer.end();
    m_IsRecording = false;
}
#pragma endregion

#pragma region Image Operations
void CommandBuffer::TransitionLayout(Texture* texture, vk::ImageLayout newLayout, u32 srcQueueFamily /*= VK_QUEUE_FAMILY_IGNORED*/, u32 dstQueueFamily /*= VK_QUEUE_FAMILY_IGNORED*/)
{
    TransitionLayout(texture,
                     texture->GetLayout(), newLayout,
                     0, texture->GetMipLevels(),
                     0, texture->GetNumLayers(),
                     srcQueueFamily, dstQueueFamily, true);
}

void CommandBuffer::TransitionLayoutMips(Texture* texture, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, u32 baseMip, u32 mipLevels, u32 srcQueueFamily /*= VK_QUEUE_FAMILY_IGNORED*/, u32 dstQueueFamily /*= VK_QUEUE_FAMILY_IGNORED*/)
{
    TransitionLayout(texture,
                     oldLayout, newLayout,
                     baseMip, mipLevels,
                     0, texture->GetNumLayers(),
                     srcQueueFamily, dstQueueFamily, false);
}

void CommandBuffer::TransitionLayout(Texture* texture,
                                     vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                     u32 baseMip, u32 mipLevels, u32 baseLayer, u32 numLayers,
                                     u32 srcQueueFamily, u32 dstQueueFamily,
                                     bool changeTextureLayout)
{
    vk::AccessFlags srcAccessMask = vk::AccessFlagBits::eNone;
    vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eNone;
    vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;

    static constexpr vk::PipelineStageFlags depthStageMask =
        (vk::PipelineStageFlagBits)0 | vk::PipelineStageFlagBits::eEarlyFragmentTests |
        vk::PipelineStageFlagBits::eLateFragmentTests;

    static constexpr vk::PipelineStageFlags sampledStageMask =
        (vk::PipelineStageFlagBits)0 | vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader |
        vk::PipelineStageFlagBits::eComputeShader;

    switch (oldLayout)
    {
    case vk::ImageLayout::eUndefined:
        break;

    case vk::ImageLayout::eGeneral:
        sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
        break;

    case vk::ImageLayout::eColorAttachmentOptimal:
        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        sourceStage = depthStageMask;
        srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
        sourceStage = depthStageMask | sampledStageMask;
        break;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        sourceStage = sampledStageMask;
        break;

    case vk::ImageLayout::eTransferSrcOptimal:
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        break;

    case vk::ImageLayout::eTransferDstOptimal:
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;

    case vk::ImageLayout::ePreinitialized:
        sourceStage = vk::PipelineStageFlagBits::eHost;
        srcAccessMask = vk::AccessFlagBits::eHostWrite;
        break;

    case vk::ImageLayout::ePresentSrcKHR:
        break;

    default:
        LNE_ASSERT(false, "Unknown image layout.");
        break;
    }

    switch (newLayout)
    {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::eFragmentDensityMapOptimalEXT:
        destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        break;

    case vk::ImageLayout::eColorAttachmentOptimal:
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dstAccessMask =
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        destinationStage = depthStageMask;
        dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
        destinationStage = depthStageMask | sampledStageMask;
        dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
        break;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        destinationStage = sampledStageMask;
        dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
        break;

    case vk::ImageLayout::eTransferSrcOptimal:
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
        dstAccessMask = vk::AccessFlagBits::eTransferRead;
        break;

    case vk::ImageLayout::eTransferDstOptimal:
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
        dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;

    case vk::ImageLayout::ePresentSrcKHR:
        break;

    default:
        LNE_ASSERT(false, "Unknown image layout.");
        break;
    }

    const vk::ImageAspectFlags aspectMask =
        texture->IsDepth() ? vk::ImageAspectFlagBits::eDepth
        : (texture->IsStencil() ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor);

    vk::ImageMemoryBarrier barrier(
        srcAccessMask,
        dstAccessMask,
        oldLayout,
        newLayout,
        srcQueueFamily,
        dstQueueFamily,
        texture->GetImage(),
        vk::ImageSubresourceRange(aspectMask, baseMip, mipLevels, baseLayer, numLayers)
    );

    m_CommandBuffer.pipelineBarrier(
        sourceStage, destinationStage, 
        vk::DependencyFlags(), nullptr, nullptr,
        barrier
    );

    if (changeTextureLayout)
        texture->m_Layout = newLayout;
}

void CommandBuffer::CopyBufferToImage(const BufferAllocation& bufferAlloc, Texture* texture, vk::ImageLayout dstLayout, std::span<vk::BufferImageCopy> regions)
{
    m_CommandBuffer.copyBufferToImage(bufferAlloc.Buffer, texture->GetImage(), dstLayout, regions);
}

void CommandBuffer::GenerateMips(Texture* texture)
{
    LNE_ASSERT(m_QueueType == EQueueFamilyType::Graphics, "Mip generation must be done on a graphics queue.");
    TransitionLayout(texture, vk::ImageLayout::eTransferSrcOptimal);

    s32 width = texture->GetDimensions().width;
    s32 height = texture->GetDimensions().height;

    for (u32 i = 1; i < texture->GetMipLevels(); i++)
    {
        TransitionLayoutMips(texture, texture->GetLayout(), vk::ImageLayout::eTransferDstOptimal, i, 1);
        vk::ImageBlit blit{};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.layerCount = texture->GetNumLayers();
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcOffsets[1] = vk::Offset3D{ width, height, 1 };

        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);

        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.layerCount = texture->GetNumLayers();
        blit.dstSubresource.mipLevel = i;
        blit.dstOffsets[1] = vk::Offset3D{ width, height, 1 };

        m_CommandBuffer.blitImage(
            texture->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
            texture->GetImage(), vk::ImageLayout::eTransferDstOptimal,
            blit,
            vk::Filter::eLinear
        );

        TransitionLayoutMips(texture, vk::ImageLayout::eTransferDstOptimal, texture->GetLayout(), i, 1);
    }
}

void CommandBuffer::Blit(Texture* src, Texture* dst)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
        vk::ImageLayout srcLayout = src->GetLayout();
    vk::ImageLayout dstLayout = dst->GetLayout();
    TransitionLayout(src, vk::ImageLayout::eTransferSrcOptimal);
    TransitionLayout(dst, vk::ImageLayout::eTransferDstOptimal);

    auto srcExtent = src->GetDimensions();
    auto dstExtent = dst->GetDimensions();
    vk::ImageBlit blit{
        vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor,
            0,
            0,
            1
        },
        {
            vk::Offset3D{ 0, 0, 0 },
            vk::Offset3D{ (int)srcExtent.width, (int)srcExtent.height, 1 }
        },
        vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor,
            0,
            0,
            1
        },
        {
            vk::Offset3D{ 0, 0, 0 },
            vk::Offset3D{ (int)dstExtent.width, (int)dstExtent.height, 1 }
        }
    };

    m_CommandBuffer.blitImage(
        src->m_Allocation.Image, vk::ImageLayout::eTransferSrcOptimal,
        dst->m_Allocation.Image, vk::ImageLayout::eTransferDstOptimal,
        1, &blit, vk::Filter::eLinear
    );

    if (srcLayout != vk::ImageLayout::eUndefined)
        TransitionLayout(src, srcLayout);
    if (dstLayout != vk::ImageLayout::eUndefined)
        TransitionLayout(dst, dstLayout);
}

void CommandBuffer::CopyBuffer(const BufferAllocation& srcBuffer, const BufferAllocation& dstBuffer, std::span<vk::BufferCopy> regions)
{
    m_CommandBuffer.copyBuffer(srcBuffer.Buffer, dstBuffer.Buffer, regions);
}
#pragma endregion

#pragma region Frame Operations
void CommandBuffer::SetViewport(const vk::Viewport& viewport)
{
    m_CommandBuffer.setViewport(0, viewport);
}

void CommandBuffer::SetScissor(const vk::Rect2D& scissor)
{
    m_CommandBuffer.setScissor(0, scissor);
}
#pragma endregion

#pragma region Utilities
void CommandBuffer::PushLabel(std::string_view label, const glm::vec4& color)
{
    vk::DebugUtilsLabelEXT labelInfo;
    labelInfo.pLabelName = label.data();
    labelInfo.color[0] = color.r; labelInfo.color[1] = color.g;
    labelInfo.color[2] = color.b; labelInfo.color[3] = color.a;
    m_CommandBuffer.beginDebugUtilsLabelEXT(labelInfo);
}

void CommandBuffer::PopLabel()
{
    m_CommandBuffer.endDebugUtilsLabelEXT();
}
#pragma endregion

}
