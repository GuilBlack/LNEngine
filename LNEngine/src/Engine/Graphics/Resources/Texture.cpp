#include "Texture.h"
#include "Graphics/GfxContext.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "Graphics/CommandPoolManager.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Renderer.h"
#include "Graphics/DynamicDescriptorAllocator.h"

namespace lne
{


SafePtr<Texture> Texture::CreateDepthTexture(
    SafePtr<class GfxContext> ctx,
    u32 width, u32 height,
    TextureUsageType::Enum usage,
    const std::string& name)
{
    vk::ImageUsageFlags flags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vkut::GetImageUsageFlags(usage);

    vk::ImageCreateInfo imageInfo(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format::eD32Sfloat,
        vk::Extent3D(width, height, 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        flags,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    );

    return SafePtr<Texture>(lnnew Texture(ctx, imageInfo, usage, ctx->GetDepthSampler(), name));
}

SafePtr<Texture> Texture::CreateColorAttachmentTexture(
    SafePtr<class GfxContext> ctx,
    u32 width, u32 height, vk::Format format,
    TextureUsageType::Enum usage,
    const std::string& name, bool useMips)
{
    vk::ImageUsageFlags flags = vk::ImageUsageFlagBits::eColorAttachment | vkut::GetImageUsageFlags(usage)
        | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;

    vk::ImageCreateInfo imageInfo(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        vk::Extent3D(width, height, 1),
        useMips ? GetMaxMipLevels(width, height) : 1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        flags,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    );
    return SafePtr<Texture>(lnnew Texture(ctx, imageInfo, usage, {}, name));
}

SafePtr<Texture> Texture::CreateColorTexture2D(
    SafePtr<class GfxContext> ctx,
    u32 width, u32 height, vk::Format format,
    TextureUsageType::Enum usage,
    bool generateMips,
    const std::string& name)
{
    vk::ImageUsageFlags flags = vkut::GetImageUsageFlags(usage) | vk::ImageUsageFlagBits::eTransferDst;

    if (generateMips)
        flags |= vk::ImageUsageFlagBits::eTransferSrc;
    vk::ImageCreateInfo imageInfo(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        vk::Extent3D(width, height, 1),
        generateMips ? GetMaxMipLevels(width, height) : 1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        flags,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    );
    return SafePtr<Texture>(lnnew Texture(ctx, imageInfo, usage, {}, name));
}

SafePtr<Texture> Texture::CreateCubemapTexture(
    SafePtr<class GfxContext> ctx,
    u32 width, u32 height, vk::Format format,
    TextureUsageType::Enum usage,
    bool generateMips,
    const std::string& name)
{
    vk::ImageUsageFlags flags = vkut::GetImageUsageFlags(usage) | vk::ImageUsageFlagBits::eTransferDst;
    if (generateMips)
        flags |= vk::ImageUsageFlagBits::eTransferSrc;
    vk::ImageCreateInfo imageInfo = vk::ImageCreateInfo{
        vk::ImageCreateFlagBits::eCubeCompatible,
        vk::ImageType::e2D,
        format,
        vk::Extent3D(width, height, 1),
        generateMips ? GetMaxMipLevels(width, height) : 1,
        6,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        flags,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    };
    return SafePtr<Texture>(lnnew Texture(ctx, imageInfo, usage, {}, name));
}

Texture::Texture(SafePtr<class GfxContext> ctx, vk::Image image, vk::Format format, vk::Extent3D extents, u32 numlayers, const std::string& name)
    : m_Context{ ctx }
    , m_Format{ format }
    , m_Extents{ extents }
    , m_NumLayers{ numlayers }
    , m_Name{ name }
    , m_OwnsImage{ false }
{
    m_Context->SetVkObjectName(image, std::format("Image: {}", name));
    const vk::ImageAspectFlags aspectMask =
        IsDepth() ? vk::ImageAspectFlagBits::eDepth
        : (IsStencil() ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor);

    m_Allocation.Image = image;
    
    m_ImageView = m_Context->CreateImageView(m_Allocation.Image, vk::ImageViewType::e2D, m_Format, 1, m_NumLayers, aspectMask, name);
}

Texture::Texture(SafePtr<class GfxContext> ctx, vk::ImageCreateInfo imageCI, TextureUsageType::Enum usage, vk::Sampler sampler, const std::string& name)
    : m_Context{ ctx }, m_Sampler{ sampler }
    , m_Format{ imageCI.format }, m_Extents{ imageCI.extent }
    , m_ImageType{ imageCI.imageType }, m_Tiling{ imageCI.tiling }
    , m_Layout{ imageCI.initialLayout }, m_NumLayers{ imageCI.arrayLayers }, m_MipLevels{ imageCI.mipLevels }
    , m_Name{ name }
    , m_UsageType{ usage }
    , m_OwnsImage{ true }
{
    if (m_MipLevels > 1)
        m_GenerateMips = true;
    vk::Device device = m_Context->GetDevice();
    VmaAllocator allocator = m_Context->GetMemoryAllocator();

    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .priority = 1.0f,
    };

    m_Context->AllocateImage(m_Allocation, imageCI, allocInfo);

    m_Context->SetVkObjectName(m_Allocation.Image, std::format("Image: {}", name));
    const vk::ImageAspectFlags aspectMask =
        IsDepth() ? vk::ImageAspectFlagBits::eDepth
        : (IsStencil() ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor);

    vk::ImageViewType viewType = vk::ImageViewType::e2D;
    if (m_ImageType == vk::ImageType::e3D)
        viewType = vk::ImageViewType::e3D;
    else if (bool(imageCI.flags & vk::ImageCreateFlagBits::eCubeCompatible) == true)
    {
        viewType = vk::ImageViewType::eCube;
        m_IsCube = true;
    }
    m_ImageView = m_Context->CreateImageView(m_Allocation.Image, viewType, m_Format,
        imageCI.mipLevels, m_NumLayers, aspectMask, std::format("ImageView: {}", name));

    switch (usage)
    {
    case TextureUsageType::eSampled:
        m_BindlessTextureHandle = m_Context->RegisterBindlessTexture(this);
        break;
    case TextureUsageType::eStorage:
        m_BindlessStorageHandle = m_Context->RegisterBindlessImage(m_ImageView);
        break;
    case TextureUsageType::eSampledAndStorage:
        m_BindlessTextureHandle = m_Context->RegisterBindlessTexture(this);
        m_BindlessStorageHandle = m_Context->RegisterBindlessImage(m_ImageView);
        break;
    default:
        break;
    }
}

Texture::~Texture()
{
    TextureResourceDeletion textureDeletion{
        .ImageView = m_ImageView,
        .Allocation = m_Allocation,
        .UsageType = m_UsageType,
        .BindlessTextureHandle = m_BindlessTextureHandle,
        .BindlessStorageHandle = m_BindlessStorageHandle,
        .OwnsAllocation = m_OwnsImage,
    };
    ResourceDeletion deletion{
        .Type = ResourceType::eTexture,
        .Resource = textureDeletion,
    };
    m_Context->EnqueueResourceDeletion(deletion);
}

bool Texture::IsDepth()
{
    return vkut::IsDepthFormat(m_Format);
}

bool Texture::IsStencil()
{
    return vkut::IsStencilFormat(m_Format);
}

void Texture::TransitionLayout(CommandBuffer* cmdBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    u32 baseMip, u32 mipLevels,
    u32 baseLayer, u32 numLayers,
    u32 srcQueueFamily, u32 dstQueueFamily,
    bool changeTextureLayout)
{
    cmdBuffer->TransitionLayout(this, oldLayout, newLayout, baseMip, mipLevels, baseLayer, numLayers, srcQueueFamily, dstQueueFamily, changeTextureLayout);
}

void Texture::UploadData(const void* data)
{
    u32 bytesPerPixel = FormatToBytesPerPixel(m_Format);

    u64 imageSize = m_Extents.width * m_Extents.height * bytesPerPixel;

    if (m_NumLayers > 1)
        imageSize *= m_NumLayers;

    LNE_ASSERT(imageSize > 0, "Invalid image size");

    BufferAllocation stagingBuffer = m_Context->AllocateStagingBuffer(imageSize);

    memcpy(stagingBuffer.AllocationInfo.pMappedData, data, imageSize);

    auto& cpManager = m_Context->GetCommandPoolManager();
    CommandBuffer* cmdBuffer = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Transfer);

    TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferDstOptimal);

    std::vector<vk::BufferImageCopy> regions;
    for (u32 layer = 0; layer < m_NumLayers; layer++)
    {
        regions.emplace_back(vk::BufferImageCopy{
            m_Extents.width * m_Extents.height * bytesPerPixel * layer,
            0,
            0,
            vk::ImageSubresourceLayers
            {
                vk::ImageAspectFlagBits::eColor, 
                0,
                layer,
                1
            },
            vk::Offset3D(0, 0, 0),
            m_Extents
        });
    }

    cmdBuffer->CopyBufferToImage(stagingBuffer, this, vk::ImageLayout::eTransferDstOptimal, regions);

    cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Transfer);

    m_Context->FreeBufferAllocation(stagingBuffer);

    cmdBuffer = ApplicationBase::GetRenderer().GetGfxContext()->GetPrimaryCommandBuffer();

    if (m_GenerateMips)
        cmdBuffer->GenerateMips(this);

    cmdBuffer->TransitionLayout(this, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Texture::UploadData(CommandBuffer* cmdBuffer, BufferAllocation stagingBuffer, const void* data, s32 size, bool autoTransitionLayout)
{
    u64 imageSize{};
    u32 bytesPerPixel{};
    if (size == -1)
    {
        bytesPerPixel = FormatToBytesPerPixel(m_Format);
        imageSize = m_Extents.width * m_Extents.height * bytesPerPixel;
    }
    else
    {
        imageSize = size;
        bytesPerPixel = (u32)imageSize / (m_Extents.width * m_Extents.height * m_NumLayers);
    }

    if (m_NumLayers > 1)
        imageSize *= m_NumLayers;
    LNE_ASSERT(imageSize > 0, "Invalid image size");
    
    memcpy(stagingBuffer.AllocationInfo.pMappedData, data, imageSize);

    if (autoTransitionLayout)
        cmdBuffer->TransitionLayout(this, vk::ImageLayout::eTransferDstOptimal);

    std::vector<vk::BufferImageCopy> regions;
    for (u32 layer = 0; layer < m_NumLayers; layer++)
    {
        regions.emplace_back(vk::BufferImageCopy{
            m_Extents.width * m_Extents.height * bytesPerPixel * layer,
            0,
            0,
            vk::ImageSubresourceLayers
            {
                vk::ImageAspectFlagBits::eColor,
                0,
                layer,
                1
            },
            vk::Offset3D(0, 0, 0),
            m_Extents
        });
    }

    cmdBuffer->CopyBufferToImage(stagingBuffer, this, vk::ImageLayout::eTransferDstOptimal, regions);

    if (autoTransitionLayout)
    {
        TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferDstOptimal,
                         m_Context->GetQueueFamilyIndex(EQueueFamilyType::Transfer),
                         m_Context->GetQueueFamilyIndex(EQueueFamilyType::Graphics));
    }
}

constexpr u32 Texture::FormatToBytesPerPixel(vk::Format format)
{
    switch (format)
    {
    // 8-bit per channel formats (RGBA and RGB)
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Srgb:
        return 4;
    case vk::Format::eR8G8B8Unorm:
    case vk::Format::eR8G8B8Srgb:
        return 3;

    // 16-bit per channel formats (half-float)
    case vk::Format::eR16G16B16A16Unorm:
    case vk::Format::eR16G16B16A16Snorm:
    case vk::Format::eR16G16B16A16Sfloat:
        return 8;
    case vk::Format::eR16G16Unorm:
    case vk::Format::eR16G16Snorm:
    case vk::Format::eR16G16Sfloat:
        return 4;
    case vk::Format::eR16Unorm:
    case vk::Format::eR16Snorm:
    case vk::Format::eR16Sfloat:
        return 2;

    // 32-bit float formats
    case vk::Format::eR32G32B32A32Sfloat:
        return 16;
    case vk::Format::eR32G32B32Sfloat:
        return 12;
    case vk::Format::eR32G32Sfloat:
        return 8;
    case vk::Format::eR32Sfloat:
        return 4;

    // Depth/stencil formats
    case vk::Format::eD16Unorm:
        return 2;
    case vk::Format::eD32Sfloat:
        return 4;
    case vk::Format::eD24UnormS8Uint:
        return 4;
    case vk::Format::eD32SfloatS8Uint:
        return 5;

    // Packed formats (careful, not byte aligned)
    case vk::Format::eB10G11R11UfloatPack32:
        return 4;
    case vk::Format::eE5B9G9R9UfloatPack32:
        return 4;
    case vk::Format::eA2B10G10R10UnormPack32:
    case vk::Format::eA2R10G10B10UnormPack32:
        return 4;

    default:
        LNE_ASSERT(false, "Unsupported format, must implement it");
        return 0;
    }
}

vk::ImageView Texture::CreateImageViewForMip(u32 mipLevel) const
{
    vk::ImageViewType viewType = vk::ImageViewType::e2D;
    if (m_ImageType == vk::ImageType::e3D)
        viewType = vk::ImageViewType::e3D;
    else if (m_IsCube)
        viewType = vk::ImageViewType::eCube;

    vk::ImageViewCreateInfo viewInfo(
        vk::ImageViewCreateFlags(),
        m_Allocation.Image,
        viewType,
        m_Format,
        vk::ComponentMapping(),
        vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            mipLevel, 1,
            0, m_NumLayers
        )
    );
    return m_Context->GetDevice().createImageView(viewInfo);
}

}
