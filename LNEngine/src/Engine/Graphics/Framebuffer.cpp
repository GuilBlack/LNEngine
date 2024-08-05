#include "lnepch.h"
#include "Framebuffer.h"
#include "GfxContext.h"
#include "Texture.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"

namespace lne
{
Framebuffer::Framebuffer(std::shared_ptr<class GfxContext> ctx, std::vector<AttachmentDesc> attachments, AttachmentDesc depth)
    : m_Context{ ctx }
    , m_ColorAttachments{ attachments }
    , m_DepthAttachment{ depth }
{
    LNE_ASSERT(m_ColorAttachments.size() > 0, "Framebuffer must have at least one color attachment");
    if (m_DepthAttachment.Texture != nullptr)
        m_HasDepth = true;
}

void Framebuffer::SetClearColor(const vk::ClearColorValue& color)
{
    for (auto& attachment : m_ColorAttachments)
    {
        attachment.ClearValue.color = color;
    }

}

void Framebuffer::Bind(vk::CommandBuffer cmdBuffer) const
{
    std::vector<vk::RenderingAttachmentInfo> colorRenderingAttachments;
    colorRenderingAttachments.reserve(m_ColorAttachments.size());

    for (auto& colorRenderingAttachmentInfo : m_ColorAttachments)
    {
        colorRenderingAttachmentInfo.Texture->TransitionLayout(cmdBuffer, colorRenderingAttachmentInfo.InitialLayout);

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
    if (m_DepthAttachment.Texture != nullptr)
    {
        m_DepthAttachment.Texture->TransitionLayout(cmdBuffer, m_DepthAttachment.InitialLayout);
        depthRenderingAttachmentInfo = vk::RenderingAttachmentInfo(
            m_DepthAttachment.Texture->GetImageView(),
            m_DepthAttachment.InitialLayout,
            vk::ResolveModeFlagBits::eNone,
            nullptr,
            vk::ImageLayout::eUndefined,
            m_DepthAttachment.LoadOp,
            m_DepthAttachment.StoreOp,
            m_DepthAttachment.ClearValue
        );
    }

    auto texture = m_ColorAttachments[0].Texture;
    vk::RenderingInfo renderingInfo = vk::RenderingInfo(
        vk::RenderingFlags{},
        vk::Rect2D{ {0,0}, {texture->GetDimensions().width, texture->GetDimensions().height} },
        texture->GetNumLayers(),
        0,
        colorRenderingAttachments
    );

    cmdBuffer.beginRendering(renderingInfo);
}

void Framebuffer::Unbind(vk::CommandBuffer cmdBuffer) const
{
    cmdBuffer.endRendering();

    for (const auto& attachment : m_ColorAttachments)
        attachment.Texture->TransitionLayout(cmdBuffer, attachment.FinalLayout);

    if (m_DepthAttachment.Texture != nullptr)
        m_DepthAttachment.Texture->TransitionLayout(cmdBuffer, m_DepthAttachment.FinalLayout);
}
}
