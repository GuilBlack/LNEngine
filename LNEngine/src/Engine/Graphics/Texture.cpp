#include "Texture.h"
#include "GfxContext.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"

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
}

Texture::~Texture()
{
    vk::Device device = m_Context->GetDevice();
    device.destroyImageView(m_ImageView);
    if (m_OwnsImage)
        m_Context->FreeImage(m_Allocation);
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

void Texture::TransitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout newLayout)
{
    if (m_Layout == newLayout)
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

    switch (m_Layout)
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
        m_Layout,
        newLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        m_Allocation.Image,
        vk::ImageSubresourceRange(aspectMask, 0, m_MipLevels, 0, m_NumLayers)
    );

    cmdBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags(), nullptr, nullptr, barrier);

    m_Layout = newLayout;
}

}
