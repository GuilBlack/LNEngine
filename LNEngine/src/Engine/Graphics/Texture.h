#pragma once
#include <vma/vk_mem_alloc.h>

namespace lne
{
class Texture
{
public:
    explicit Texture(std::shared_ptr<class GfxContext> ctx, vk::Image image,
        vk::Format format, vk::Extent3D extents, uint32_t numlayers = 1, const std::string& name = "");
    ~Texture();

    [[nodiscard]] vk::ImageView GetImageView() const { return m_ImageView; }
    [[nodiscard]] vk::Image GetImage() const { return m_Image; }
    [[nodiscard]] vk::DeviceSize GetSize() const { return m_Size; }
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
    std::shared_ptr<class GfxContext> m_Context;
    VmaAllocation m_VmaAllocation = nullptr;
    vk::Image m_Image{};
    vk::ImageView m_ImageView{};
    vk::DeviceSize m_Size{};
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