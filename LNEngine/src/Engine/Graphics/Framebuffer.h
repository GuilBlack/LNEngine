#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Resources/Texture.h"
#include "Engine/Core/Utils/Defines.h"

namespace lne
{
class CommandBuffer;

struct AttachmentDesc
{
    SafePtr<class Texture> Texture;
    vk::AttachmentLoadOp LoadOp = vk::AttachmentLoadOp::eDontCare;
    vk::AttachmentStoreOp StoreOp = vk::AttachmentStoreOp::eDontCare;
    vk::ImageLayout InitialLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout FinalLayout = vk::ImageLayout::eUndefined;
    vk::ClearValue ClearValue;
};

class Framebuffer
{
public:
    Framebuffer() = default;
    Framebuffer(
        std::vector<AttachmentDesc> attachments,
        AttachmentDesc depth = {});
    ~Framebuffer() = default;

    void                                    Init(std::vector<AttachmentDesc> attachments,
                                                 AttachmentDesc depth = {});

    void                                    SetClearColor(const vk::ClearColorValue& color);
    void                                    ChangeColorAttachmentsOps(vk::AttachmentLoadOp loadOp,
                                                                      vk::AttachmentStoreOp storeOp);

    [[nodiscard]] std::vector<AttachmentDesc>& GetColorAttachments() { return m_ColorAttachments; }
    [[nodiscard]] AttachmentDesc&           GetDepthAttachment() { return m_DepthAttachment; }
    [[nodiscard]] vk::CommandBufferInheritanceRenderingInfo GetInheritanceRenderingInfo() const;
    [[nodiscard]] vk::Extent3D              GetExtent() const;
    [[nodiscard]] u32                       GetLayerCount() const;
    [[nodiscard]] bool                      HasDepth() const { return m_HasDepth; }


private:
    std::vector<AttachmentDesc>         m_ColorAttachments;
    std::vector<vk::Format>             m_ColorFormats{};
    AttachmentDesc                      m_DepthAttachment;
    bool                                m_HasDepth{ false };
};
}
