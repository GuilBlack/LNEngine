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
    static SafePtr<Texture> CreateColorTexture2D(SafePtr<class GfxContext> ctx, uint32_t width, uint32_t height, bool generateMips = true, const std::string& name = "");
    static SafePtr<Texture> CreateCubemapTexture(SafePtr<class GfxContext> ctx, uint32_t width, uint32_t height, bool generateMips = true, const std::string& name = "");
    static constexpr uint32_t GetMaxMipLevels(uint32_t width, uint32_t height)
    {
        uint32_t mipLevels = 1;
        while (width > 1 && height > 1)
        {
            width >>= 1;
            height >>= 1;
            mipLevels++;
        }
        return mipLevels;
    }
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
    [[nodiscard]] bool ShouldGenerateMips() const { return m_GenerateMips; }
    [[nodiscard]] vk::Sampler GetSampler() const { return m_Sampler; }
    [[nodiscard]] uint32_t GetBindlessHandle() const { return m_BindlessHandle; }
    [[nodiscard]] const std::string& GetName() const { return m_Name; }

    [[nodiscard]] bool IsDepth();
    [[nodiscard]] bool IsStencil();

    inline void TransitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout newLayout,uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        TransitionLayout(cmdBuffer, m_Layout, newLayout, 0, m_MipLevels, 0, m_NumLayers, srcQueueFamily, dstQueueFamily, true);
    }
    inline void TransitionLayoutMips(vk::CommandBuffer cmdBuffer, 
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout, 
        uint32_t baseMip, uint32_t mipLevels,
        uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        TransitionLayout(cmdBuffer, oldLayout, newLayout, baseMip, mipLevels, 0, m_NumLayers, srcQueueFamily, dstQueueFamily, false);
    }
    inline void TransitionLayoutLayers(vk::CommandBuffer cmdBuffer, 
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout, 
        uint32_t baseLayer, uint32_t numLayers,
        uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        TransitionLayout(cmdBuffer, oldLayout, newLayout, 0, m_MipLevels, baseLayer, numLayers, srcQueueFamily, dstQueueFamily, false);
    }
    void TransitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        uint32_t baseMip, uint32_t mipLevels,
        uint32_t baseLayer, uint32_t numLayers,
        uint32_t srcQueueFamily, uint32_t dstQueueFamily,
        bool changeTextureLayout);

    void GenerateMipmaps(vk::CommandBuffer cmdBuffer);

    void UploadData(const void* data);
    void UploadData(vk::CommandBuffer cmdBuffer, BufferAllocation stagingBuffer, const void* data);

private:
    SafePtr<class GfxContext> m_Context;
    ImageAllocation m_Allocation{};
    vk::ImageView m_ImageView{};
    vk::Sampler m_Sampler{};
    uint32_t m_BindlessHandle{ 0 };
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

private:
    constexpr uint32_t FormatToBytesPerPixel(vk::Format format);
};
}
