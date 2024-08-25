#include "Texture.h"
#include "GfxContext.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "CommandBufferManager.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "DynamicDescriptorAllocator.h"

namespace lne
{
SafePtr<Texture> Texture::CreateDepthTexture(SafePtr<class GfxContext> ctx, uint32_t width, uint32_t height, const std::string& name)
{
    vk::ImageCreateInfo imageInfo(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format::eD32Sfloat,
        vk::Extent3D(width, height, 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    );

    return SafePtr<Texture>(new Texture(ctx, imageInfo, name));
}

SafePtr<Texture> Texture::CreateColorTexture2D(SafePtr<class GfxContext> ctx, uint32_t width, uint32_t height, bool generateMips, const std::string& name)
{
    vk::ImageUsageFlags flags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    if (generateMips)
        flags |= vk::ImageUsageFlagBits::eTransferSrc;
    vk::ImageCreateInfo imageInfo(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format::eR8G8B8A8Unorm,
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
    return SafePtr<Texture>(lnnew Texture(ctx, imageInfo, name));
}

Texture::Texture(SafePtr<class GfxContext> ctx, vk::Image image, vk::Format format, vk::Extent3D extents, uint32_t numlayers, const std::string& name)
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

Texture::Texture(SafePtr<class GfxContext> ctx, vk::ImageCreateInfo imageCI, const std::string& name)
    : m_Context{ ctx }
    , m_Format{ imageCI.format }
    , m_Extents{ imageCI.extent }
    , m_ImageType{ imageCI.imageType }
    , m_Tiling{ imageCI.tiling }
    , m_Layout{ imageCI.initialLayout }
    , m_NumLayers{ imageCI.arrayLayers }
    , m_MipLevels{ imageCI.mipLevels }
    , m_Name{ name }
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

    m_ImageView = m_Context->CreateImageView(m_Allocation.Image, vk::ImageViewType::e2D, m_Format, 
        imageCI.mipLevels, m_NumLayers, aspectMask, std::format("ImageView: {}", name));

    if (IsDepth() == false && IsStencil() == false)
        m_BindlessHandle = m_Context->RegisterBindlessTexture(this);
}

Texture::~Texture()
{
    vk::Device device = m_Context->GetDevice();
    device.destroyImageView(m_ImageView);
    if (m_OwnsImage)
        m_Context->FreeImage(m_Allocation);
    m_Context->FreeBindlessImage(m_BindlessHandle);
}

bool Texture::IsDepth()
{
    return (m_Format == vk::Format::eD16Unorm 
        || m_Format == vk::Format::eD32Sfloat 
        || m_Format == vk::Format::eD16UnormS8Uint 
        || m_Format == vk::Format::eD24UnormS8Uint 
        || m_Format == vk::Format::eD32SfloatS8Uint);
}

bool Texture::IsStencil()
{
    return (m_Format == vk::Format::eS8Uint 
        || m_Format == vk::Format::eD16UnormS8Uint 
        || m_Format == vk::Format::eD24UnormS8Uint 
        || m_Format == vk::Format::eD32SfloatS8Uint);
}

void Texture::TransitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    uint32_t baseMip, uint32_t mipLevels,
    uint32_t baseLayer, uint32_t numLayers,
    bool changeTextureLayout)
{
    if (oldLayout == newLayout)
        return;

    vk::AccessFlags srcAccessMask = vk::AccessFlagBits::eNone;
    vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eNone;
    vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;

    static constexpr vk::PipelineStageFlags depthStageMask =
        (vk::PipelineStageFlagBits)0 | vk::PipelineStageFlagBits::eEarlyFragmentTests |
        vk::PipelineStageFlagBits::eLateFragmentTests;

    static constexpr vk::PipelineStageFlags sampledStageMask =
        (vk::PipelineStageFlagBits)0 | vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader |
        vk::PipelineStageFlagBits::eComputeShader;

    switch (oldLayout)
    {
    case vk::ImageLayout::eUndefined:
        break;

    case vk::ImageLayout::eGeneral:
        sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
        break;

    case vk::ImageLayout::eColorAttachmentOptimal:
        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        sourceStage = depthStageMask;
        srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
        sourceStage = depthStageMask | sampledStageMask;
        break;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        sourceStage = sampledStageMask;
        break;

    case vk::ImageLayout::eTransferSrcOptimal:
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        break;

    case vk::ImageLayout::eTransferDstOptimal:
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;

    case vk::ImageLayout::ePreinitialized:
        sourceStage = vk::PipelineStageFlagBits::eHost;
        srcAccessMask = vk::AccessFlagBits::eHostWrite;
        break;

    case vk::ImageLayout::ePresentSrcKHR:
        break;

    default:
        LNE_ASSERT(false, "Unknown image layout.");
        break;
    }

    switch (newLayout)
    {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::eFragmentDensityMapOptimalEXT:
        destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        break;

    case vk::ImageLayout::eColorAttachmentOptimal:
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dstAccessMask =
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        destinationStage = depthStageMask;
        dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;

    case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
        destinationStage = depthStageMask | sampledStageMask;
        dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
        break;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        destinationStage = sampledStageMask;
        dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
        break;

    case vk::ImageLayout::eTransferSrcOptimal:
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
        dstAccessMask = vk::AccessFlagBits::eTransferRead;
        break;

    case vk::ImageLayout::eTransferDstOptimal:
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
        dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;

    case vk::ImageLayout::ePresentSrcKHR:
        break;

    default:
        LNE_ASSERT(false, "Unknown image layout.");
        break;
    }

    const vk::ImageAspectFlags aspectMask =
        IsDepth() ? vk::ImageAspectFlagBits::eDepth
        : (IsStencil() ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor);

    vk::ImageMemoryBarrier barrier(
        srcAccessMask,
        dstAccessMask,
        oldLayout,
        newLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        m_Allocation.Image,
        vk::ImageSubresourceRange(aspectMask, baseMip, mipLevels, baseLayer, numLayers)
    );

    cmdBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags(), nullptr, nullptr, barrier);

    if (changeTextureLayout)
        m_Layout = newLayout;
}

void Texture::UploadData(const void* data)
{
    uint32_t bytesPerPixel = FormatToBytesPerPixel(m_Format);

    uint64_t imageSize = m_Extents.width * m_Extents.height * bytesPerPixel;

    LNE_ASSERT(imageSize > 0, "Invalid image size");

    BufferAllocation stagingBuffer = m_Context->AllocateStagingBuffer(imageSize);

    memcpy(stagingBuffer.AllocationInfo.pMappedData, data, imageSize);

    vk::CommandBuffer cmdBuffer = m_Context->GetTransferCommandBufferManager().BeginSingleTimeCommands();

    TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferDstOptimal);

    vk::BufferImageCopy region(
        0,
        0,
        0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, m_NumLayers),
        vk::Offset3D(0, 0, 0),
        m_Extents
    );

    cmdBuffer.copyBufferToImage(stagingBuffer.Buffer, m_Allocation.Image, vk::ImageLayout::eTransferDstOptimal, region);
    
    m_Context->GetTransferCommandBufferManager().EndSingleTimeCommands();

    m_Context->FreeBuffer(stagingBuffer);

    cmdBuffer = ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer();
    
    if (m_GenerateMips)
        GenerateMipmaps(cmdBuffer);

    TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
}

constexpr uint32_t Texture::FormatToBytesPerPixel(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eR8G8B8A8Unorm:
        return 4;
    case vk::Format::eR8G8B8A8Srgb:
        return 4;
    default:
        LNE_ASSERT(false, "Unsupported format, must implement it");
        return 0;
    }
}

void Texture::GenerateMipmaps(vk::CommandBuffer cmdBuffer)
{
    TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferSrcOptimal);

    int32_t width = m_Extents.width;
    int32_t height = m_Extents.height;

    for (uint32_t i = 1; i < m_MipLevels; i++)
    {
        TransitionLayoutMips(cmdBuffer, m_Layout, vk::ImageLayout::eTransferDstOptimal, i, 1);
        vk::ImageBlit blit{};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.layerCount = m_NumLayers;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcOffsets[1] = vk::Offset3D{ width, height, 1 };

        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);

        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.layerCount = m_NumLayers;
        blit.dstSubresource.mipLevel = i;
        blit.dstOffsets[1] = vk::Offset3D{ width, height, 1 };

        cmdBuffer.blitImage(
            m_Allocation.Image, vk::ImageLayout::eTransferSrcOptimal,
            m_Allocation.Image, vk::ImageLayout::eTransferDstOptimal,
            blit,
            vk::Filter::eLinear
        );

        TransitionLayoutMips(cmdBuffer, vk::ImageLayout::eTransferDstOptimal, m_Layout, i, 1);
    }
}

}
