#include "lnepch.h"
#include "Framebuffer.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "Graphics/GfxContext.h"
#include "Graphics/CommandBuffer.h"
#include "Graphics/Resources/Texture.h"

namespace lne
{
Framebuffer::Framebuffer(std::vector<AttachmentDesc> attachments, AttachmentDesc depth)
    : m_ColorAttachments{ attachments }
    , m_DepthAttachment{ depth }
{
    m_ColorFormats.reserve(m_ColorAttachments.size());
    for (const auto& colorAttachment : m_ColorAttachments)
        m_ColorFormats.push_back(colorAttachment.Texture->GetFormat());
    if (m_DepthAttachment.Texture != nullptr)
        m_HasDepth = true;
}

void Framebuffer::Init(std::vector<AttachmentDesc> attachments, AttachmentDesc depth)
{
    m_ColorAttachments = attachments;
    m_ColorFormats.clear();
    m_ColorFormats.reserve(m_ColorAttachments.size());
    for (const auto& colorAttachment : m_ColorAttachments)
        m_ColorFormats.push_back(colorAttachment.Texture->GetFormat());

    m_DepthAttachment = depth;
    if (m_DepthAttachment.Texture != nullptr)
        m_HasDepth = true;
}

void Framebuffer::SetClearColor(const vk::ClearColorValue& color)
{
    for (auto& attachment : m_ColorAttachments)
        attachment.ClearValue.color = color;
}

void Framebuffer::ChangeColorAttachmentsOps(vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp)
{
    for (auto& attachment : m_ColorAttachments)
    {
        attachment.LoadOp = loadOp;
        attachment.StoreOp = storeOp;
    }
}

vk::CommandBufferInheritanceRenderingInfo Framebuffer::GetInheritanceRenderingInfo() const
{
    vk::CommandBufferInheritanceRenderingInfo info{};
    info.setColorAttachmentFormats(m_ColorFormats);
    if (m_HasDepth)
        info.setDepthAttachmentFormat(m_DepthAttachment.Texture->GetFormat());
    info.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    return info;
}

vk::Extent3D Framebuffer::GetExtent() const
{
    if (m_ColorAttachments.size() > 0)
        return m_ColorAttachments[0].Texture->GetDimensions();
    
    if (m_DepthAttachment.Texture != nullptr)
        return m_DepthAttachment.Texture->GetDimensions();
    return {};
}

u32 Framebuffer::GetLayerCount() const
{
    if (m_ColorAttachments.size() > 0)
        return m_ColorAttachments[0].Texture->GetNumLayers();

    if (m_DepthAttachment.Texture != nullptr)
        return m_DepthAttachment.Texture->GetNumLayers();
    return {};
}
}
