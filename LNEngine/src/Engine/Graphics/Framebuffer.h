#pragma once

namespace lne
{

struct AttachmentDesc
{
    std::shared_ptr<class Texture> Texture = nullptr;
    vk::AttachmentLoadOp LoadOp = vk::AttachmentLoadOp::eDontCare;
    vk::AttachmentStoreOp StoreOp = vk::AttachmentStoreOp::eDontCare;
    vk::ImageLayout InitialLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout FinalLayout = vk::ImageLayout::eUndefined;
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

    void SetClearColor(const vk::ClearColorValue& color);

    void Bind(vk::CommandBuffer cmdBuffer) const;
    void Unbind(vk::CommandBuffer cmdBuffer) const;

    [[nodiscard]] const std::vector<AttachmentDesc>& GetColorAttachments() const { return m_ColorAttachments; }
    [[nodiscard]] const AttachmentDesc& GetDepthDepth() const { return m_DepthAttachment; }

private:
    std::shared_ptr<class GfxContext> m_Context;
    std::vector<AttachmentDesc> m_ColorAttachments;
    AttachmentDesc m_DepthAttachment;
};
}
