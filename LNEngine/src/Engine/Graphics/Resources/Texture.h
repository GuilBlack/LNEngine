#pragma once
#include "../vendor/VMA/vk_mem_alloc.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/Utils/Defines.h"

namespace lne
{
class CommandBuffer;

class Texture : public RefCountBase
{
public:
    static SafePtr<Texture> CreateDepthTexture(
        SafePtr<class GfxContext> ctx, 
        u32 width, u32 height,
        TextureUsageType::Enum usage = TextureUsageType::eSampled, 
        const std::string& name = ""
    );
    static SafePtr<Texture> CreateColorAttachmentTexture(
        SafePtr<class GfxContext> ctx, 
        u32 width, u32 height, vk::Format format,
        TextureUsageType::Enum usage = TextureUsageType::eSampled,
        const std::string& name = "", bool useMips = false
    );
    static SafePtr<Texture> CreateColorTexture2D(
        SafePtr<class GfxContext> ctx, 
        u32 width, u32 height, vk::Format format = vk::Format::eR8G8B8A8Srgb,
        TextureUsageType::Enum usage = TextureUsageType::eSampled, 
        bool generateMips = true, 
        const std::string& name = ""
    );
    static SafePtr<Texture> CreateCubemapTexture(
        SafePtr<class GfxContext> ctx,
        u32 width, u32 height, vk::Format format = vk::Format::eR8G8B8A8Srgb,
        TextureUsageType::Enum usage = TextureUsageType::eSampled,
        bool generateMips = true,
        const std::string& name = ""
    );

    static constexpr u32 GetMaxMipLevels(u32 width, u32 height)
    {
        u32 mipLevels = 1;
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
        vk::Format format, vk::Extent3D extents, u32 numlayers = 1, const std::string& name = "");
    explicit Texture(SafePtr<class GfxContext> ctx, vk::ImageCreateInfo imageCI, TextureUsageType::Enum usage, vk::Sampler = {}, const std::string& name = "");
    virtual ~Texture();

    [[nodiscard]] vk::ImageView             GetImageView() const { return m_ImageView; }
    [[nodiscard]] vk::Image                 GetImage() const { return m_Allocation.Image; }
    [[nodiscard]] vk::Extent3D              GetDimensions() const { return m_Extents; }
    [[nodiscard]] vk::Format                GetFormat() const { return m_Format; }
    [[nodiscard]] vk::ImageType             GetImageType() const { return m_ImageType; }
    [[nodiscard]] vk::ImageTiling           GetTiling() const { return m_Tiling; }
    [[nodiscard]] vk::ImageLayout           GetLayout() const { return m_Layout; }
    [[nodiscard]] u32                       GetNumLayers() const { return m_NumLayers; }
    [[nodiscard]] u32                       GetMipLevels() const { return m_MipLevels; }
    [[nodiscard]] bool                      ShouldGenerateMips() const { return m_GenerateMips; }
    [[nodiscard]] vk::Sampler               GetSampler() const { return m_Sampler; }
    [[nodiscard]] BindlessImageHandle       GetBindlessTextureHandle() const { return m_BindlessTextureHandle; }
    [[nodiscard]] BindlessImageHandle       GetBindlessStorageHandle() const { return m_BindlessStorageHandle; }
    [[nodiscard]] TextureUsageType::Enum    GetUsageType() const { return m_UsageType; }
    [[nodiscard]] const std::string&        GetName() const { return m_Name; }

    [[nodiscard]] bool                      IsDepth();
    [[nodiscard]] bool                      IsStencil();

    // TODO: Move all the TransitionLayout methods to CommandBuffer.
    inline void                             TransitionLayout(CommandBuffer* cmdBuffer,
                                                             vk::ImageLayout newLayout,
                                                             u32 srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                             u32 dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        TransitionLayout(cmdBuffer, m_Layout, newLayout, 0, m_MipLevels, 0, m_NumLayers, srcQueueFamily, dstQueueFamily, true);
    }

    inline void                             TransitionLayoutMips(CommandBuffer* cmdBuffer, 
                                                                 vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                                                 u32 baseMip, u32 mipLevels,
                                                                 u32 srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                                 u32 dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        TransitionLayout(cmdBuffer, oldLayout, newLayout, baseMip, mipLevels, 0, m_NumLayers, srcQueueFamily, dstQueueFamily, false);
    }

    inline void                             TransitionLayoutLayers(CommandBuffer* cmdBuffer, 
                                                                   vk::ImageLayout oldLayout, vk::ImageLayout newLayout, 
                                                                   u32 baseLayer, u32 numLayers,
                                                                   u32 srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                                                                   u32 dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        TransitionLayout(cmdBuffer, oldLayout, newLayout, 0, m_MipLevels, baseLayer, numLayers, srcQueueFamily, dstQueueFamily, false);
    }

    void                                    TransitionLayout(CommandBuffer* cmdBuffer,
                                                             vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                                             u32 baseMip, u32 mipLevels,
                                                             u32 baseLayer, u32 numLayers,
                                                             u32 srcQueueFamily, u32 dstQueueFamily,
                                                             bool changeTextureLayout);

    vk::ImageView                           CreateImageViewForMip(u32 mipLevel) const;

    void                                    UploadData(const void* data);

    // TODO: remove the autoTransitionLayout parameter. It shouldn't be the responsibility of this method.
    void                                    UploadData(CommandBuffer* cmdBuffer,
                                                       BufferAllocation stagingBuffer, const void* data,
                                                       s32 size = -1, bool autoTransitionLayout = true);

protected:
    virtual std::string_view                GetDebugName() const { return m_Name; }

private:
    SafePtr<class GfxContext>   m_Context;
    ImageAllocation             m_Allocation{};
    vk::ImageView               m_ImageView{};
    vk::Sampler                 m_Sampler{};
    BindlessImageHandle         m_BindlessTextureHandle{ 0 };
    BindlessImageHandle         m_BindlessStorageHandle{ 0 };
    vk::Format                  m_Format{};
    vk::Extent3D                m_Extents{};
    vk::ImageType               m_ImageType{ vk::ImageType::e2D };
    vk::ImageTiling             m_Tiling{ vk::ImageTiling::eOptimal };
    vk::ImageLayout             m_Layout{ vk::ImageLayout::eUndefined };
    u32                         m_NumLayers{ 1 };
    u32                         m_MipLevels{ 1 };
    std::string                 m_Name{};
    TextureUsageType::Enum      m_UsageType{};
    bool                        m_GenerateMips{ false };
    bool                        m_OwnsImage{ true };
    bool                        m_IsCube{ false };

    friend class Renderer;
    friend class GfxLoader;
    friend class CommandBuffer;

private:
    constexpr u32                           FormatToBytesPerPixel(vk::Format format);
};
}
