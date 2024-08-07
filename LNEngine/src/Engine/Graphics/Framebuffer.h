#pragma once
#include "Engine/Core/SafePtr.h"

namespace lne
{

struct AttachmentDesc
{
    SafePtr<class Texture> Texture = nullptr;
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
        SafePtr<class GfxContext> ctx,
        std::vector<AttachmentDesc> attachments,
        AttachmentDesc depth = {});
    ~Framebuffer() = default;

    void SetClearColor(const vk::ClearColorValue& color);

    void Bind(vk::CommandBuffer cmdBuffer) const;
    void Unbind(vk::CommandBuffer cmdBuffer) const;

    [[nodiscard]] const std::vector<AttachmentDesc>& GetColorAttachments() const { return m_ColorAttachments; }
    [[nodiscard]] const AttachmentDesc& GetDepthAttachment() const { return m_DepthAttachment; }
    [[nodiscard]] bool HasDepth() const { return m_HasDepth; }


private:
    SafePtr<class GfxContext> m_Context;
    std::vector<AttachmentDesc> m_ColorAttachments;
    AttachmentDesc m_DepthAttachment;
    bool m_HasDepth = false;
};
}
