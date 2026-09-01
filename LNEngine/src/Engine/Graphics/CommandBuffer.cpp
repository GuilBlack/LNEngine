#include "CommandBuffer.h"

#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Profiling.h"

#include "Graphics/VulkanUtils.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Framebuffer.h"
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
    LNE_ASSERT(m_QueueType == EQueueFamilyType::Graphics,
               "Blit must be done on a graphics queue.");
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
    LNE_ASSERT(m_QueueType == EQueueFamilyType::Graphics,
               "SetViewport can only be called on graphics command buffers.");
    m_CommandBuffer.setViewport(0, viewport);
}

void CommandBuffer::SetScissor(const vk::Rect2D& scissor)
{
    LNE_ASSERT(m_QueueType == EQueueFamilyType::Graphics,
               "SetScissor can only be called on graphics command buffers.");
    m_CommandBuffer.setScissor(0, scissor);
}

void CommandBuffer::BeginRenderPass(Framebuffer* framebuffer)
{
    LNE_ASSERT(m_QueueType == EQueueFamilyType::Graphics,
               "BeginRenderPass can only be called on graphics command buffers.");
    LNE_ASSERT(m_BoundFramebuffer == nullptr, "A framebuffer is already bound. Unbind it before binding another one.");
    LNE_ASSERT(framebuffer != nullptr, "Cannot bind a null framebuffer.");

    m_BoundFramebuffer     = framebuffer;
    auto& colorAttachments = m_BoundFramebuffer->GetColorAttachments();
    auto& depthAttachment  = m_BoundFramebuffer->GetDepthAttachment();

    if (!(colorAttachments.size() > 0 || depthAttachment.Texture != nullptr))
    {
        LNE_WARN("Framebuffer has no attachments. Cannot bind.");
        return;
    }

    std::vector<vk::RenderingAttachmentInfo> colorRenderingAttachments;
    colorRenderingAttachments.reserve(colorAttachments.size());

    for (auto& colorRenderingAttachmentInfo : colorAttachments)
    {
        TransitionLayout(colorRenderingAttachmentInfo.Texture.GetPtr(), colorRenderingAttachmentInfo.InitialLayout);

        colorRenderingAttachments.emplace_back(vk::RenderingAttachmentInfo(
            colorRenderingAttachmentInfo.Texture->GetImageView(),
            colorRenderingAttachmentInfo.InitialLayout,
            vk::ResolveModeFlagBits::eNone,
            nullptr,
            vk::ImageLayout::eUndefined,
            colorRenderingAttachmentInfo.LoadOp,
            colorRenderingAttachmentInfo.StoreOp,
            colorRenderingAttachmentInfo.ClearValue
        ));
    }

    vk::RenderingAttachmentInfo depthRenderingAttachmentInfo;
    if (m_BoundFramebuffer->HasDepth())
    {
        TransitionLayout(depthAttachment.Texture.GetPtr(), depthAttachment.InitialLayout);
        depthRenderingAttachmentInfo = vk::RenderingAttachmentInfo(
            depthAttachment.Texture->GetImageView(),
            depthAttachment.InitialLayout,
            vk::ResolveModeFlagBits::eNone,
            nullptr,
            vk::ImageLayout::eUndefined,
            depthAttachment.LoadOp,
            depthAttachment.StoreOp,
            depthAttachment.ClearValue
        );
    }
    vk::RenderingFlags renderingFlags{};
    if (m_Type == Type::eSecondary)
    {
        renderingFlags |= vk::RenderingFlagBits::eContentsSecondaryCommandBuffers;
        renderingFlags |= vk::RenderingFlagBits::eContentsInlineEXT;
    }

    vk::Extent3D extent = m_BoundFramebuffer->GetExtent();
    vk::RenderingInfo renderingInfo = vk::RenderingInfo{
        renderingFlags,
        vk::Rect2D{ {0,0}, {extent.width, extent.height} },
        m_BoundFramebuffer->GetLayerCount(),
        0,
        colorRenderingAttachments,
        m_BoundFramebuffer->HasDepth() ? &depthRenderingAttachmentInfo : nullptr
    };

    m_CommandBuffer.beginRendering(renderingInfo);
}

void CommandBuffer::EndRenderPass()
{
    LNE_ASSERT(m_QueueType == EQueueFamilyType::Graphics,
               "EndRenderPass can only be called on graphics command buffers.");
#ifdef LNE_DEBUG
    if (m_BoundFramebuffer == nullptr)
    {
        LNE_WARN("No framebuffer is currently bound. Cannot unbind.");
        return;
    }
#endif
    auto& colorAttachments = m_BoundFramebuffer->GetColorAttachments();
    auto& depthAttachment  = m_BoundFramebuffer->GetDepthAttachment();
    if (!(colorAttachments.size() > 0 || depthAttachment.Texture != nullptr))
    {
        LNE_WARN("Framebuffer has no attachments. Cannot unbind.");
        return;
    }

    m_CommandBuffer.endRendering();

    for (auto& attachment : colorAttachments)
        TransitionLayout(attachment.Texture.GetPtr(), attachment.FinalLayout);

    if (m_BoundFramebuffer->HasDepth())
        TransitionLayout(depthAttachment.Texture.GetPtr(), depthAttachment.FinalLayout);

    m_BoundFramebuffer = nullptr;
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
