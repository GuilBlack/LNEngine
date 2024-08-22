#pragma once
#include "../vendor/VMA/vk_mem_alloc.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"

namespace lne
{
class Texture : public RefCountBase
{
public:
    static SafePtr<Texture> CreateDepthTexture(SafePtr<class GfxContext> ctx, uint32_t width, uint32_t height, const std::string& name = "");
public:
    explicit Texture(SafePtr<class GfxContext> ctx, vk::Image image,
        vk::Format format, vk::Extent3D extents, uint32_t numlayers = 1, const std::string& name = "");
    explicit Texture(SafePtr<class GfxContext> ctx, vk::ImageCreateInfo imageCI, const std::string& name = "");
    virtual ~Texture();

    [[nodiscard]] vk::ImageView GetImageView() const { return m_ImageView; }
    [[nodiscard]] vk::Image GetImage() const { return m_Allocation.Image; }
    [[nodiscard]] vk::Extent3D GetDimensions() const { return m_Extents; }
    [[nodiscard]] vk::Format GetFormat() const { return m_Format; }
    [[nodiscard]] vk::ImageType GetImageType() const { return m_ImageType; }
    [[nodiscard]] vk::ImageTiling GetTiling() const { return m_Tiling; }
    [[nodiscard]] vk::ImageLayout GetLayout() const { return m_Layout; }
    [[nodiscard]] uint32_t GetNumLayers() const { return m_NumLayers; }
    [[nodiscard]] uint32_t GetMipLevels() const { return m_MipLevels; }
    [[nodiscard]] bool GenerateMips() const { return m_GenerateMips; }
    [[nodiscard]] const std::string& GetName() const { return m_Name; }

    [[nodiscard]] bool IsDepth();
    [[nodiscard]] bool IsStencil();

    void TransitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout newLayout);

private:
    SafePtr<class GfxContext> m_Context;
    ImageAllocation m_Allocation{};
    vk::ImageView m_ImageView{};
    vk::Format m_Format{};
    vk::Extent3D m_Extents{};
    vk::ImageType m_ImageType{ vk::ImageType::e2D };
    vk::ImageTiling m_Tiling{ vk::ImageTiling::eOptimal };
    vk::ImageLayout m_Layout{ vk::ImageLayout::eUndefined };
    uint32_t m_NumLayers{ 1 };
    uint32_t m_MipLevels{ 1 };
    bool m_GenerateMips{ false };
    std::string m_Name{};
    bool m_OwnsImage{ true };
};
}
