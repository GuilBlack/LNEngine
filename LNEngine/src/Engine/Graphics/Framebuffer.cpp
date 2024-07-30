#include "lnepch.h"
#include "Framebuffer.h"
#include "GfxContext.h"
#include "Texture.h"

namespace lne
{
Framebuffer::Framebuffer(std::shared_ptr<class GfxContext> ctx, std::vector<AttachmentDesc> attachments, AttachmentDesc depth)
    : m_Context{ ctx }
    , m_ColorAttachments{ attachments }
    , m_DepthAttachment{ depth }
{}

void Framebuffer::Bind(vk::CommandBuffer cmdBuffer)
{
    std::vector<vk::ImageView> attachments;
}

void Framebuffer::Unbind(vk::CommandBuffer cmdBuffer)
{
    for (const auto& attachment : m_ColorAttachments)
        attachment.Texture->TransitionLayout(cmdBuffer, attachment.FinalLayout);
    m_DepthAttachment.Texture->TransitionLayout(cmdBuffer, m_DepthAttachment.FinalLayout);
    cmdBuffer.endRendering();
}
}
