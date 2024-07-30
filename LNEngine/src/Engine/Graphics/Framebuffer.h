#pragma once

namespace lne
{

struct AttachmentDesc
{
    std::shared_ptr<class Texture> Texture;
    vk::AttachmentLoadOp LoadOp;
    vk::AttachmentStoreOp StoreOp;
    vk::ImageLayout InitialLayout;
    vk::ImageLayout FinalLayout;
    vk::ClearValue ClearValue;
};

class Framebuffer
{
public:
    Framebuffer(
        std::shared_ptr<class GfxContext> ctx, 
        std::vector<AttachmentDesc> attachments,
        AttachmentDesc depth);
    ~Framebuffer() = default;

    void Bind(vk::CommandBuffer cmdBuffer);
    void Unbind(vk::CommandBuffer cmdBuffer);

    [[nodiscard]] const std::vector<AttachmentDesc>& GetColorAttachments() const { return m_ColorAttachments; }
    [[nodiscard]] const AttachmentDesc& GetDepthDepth() const { return m_DepthAttachment; }

private:
    std::shared_ptr<class GfxContext> m_Context;
    std::vector<AttachmentDesc> m_ColorAttachments;
    AttachmentDesc m_DepthAttachment;
};
}
