#include "SwapChain.h"
#include "GfxContext.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Log.h"

namespace lne
{
Swapchain::Swapchain(SafePtr<class GfxContext> ctx, vk::SurfaceKHR surface)
{
    m_Context = ctx;
    m_Surface = surface;
    CreateSwapchain();
    CreateSyncObjects();
}

Swapchain::~Swapchain()
{
    auto device = m_Context->GetDevice();

    device.destroySemaphore(m_Semaphores.ImageAvailable);
    device.destroySemaphore(m_Semaphores.RenderFinished);
    device.destroyFence(m_AcquireFence);

    m_ColorAttachments.clear();

    device.destroySwapchainKHR(m_Swapchain);
    m_Context->VulkanInstance().destroySurfaceKHR(m_Surface);
}

vk::SubmitInfo Swapchain::GetSubmitInfo(vk::PipelineStageFlags* waitStages, bool waitForImageAvailable, bool signalRenderFinished) const
{
    vk::SubmitInfo submitInfo(
        waitForImageAvailable ? 1 : 0,
        waitForImageAvailable ? &m_Semaphores.ImageAvailable : nullptr,
        waitForImageAvailable ? waitStages : nullptr,
        1,
        {},
        signalRenderFinished ? 1 : 0,
        signalRenderFinished ? &m_Semaphores.RenderFinished : nullptr
    );

    return submitInfo;
}

SafePtr<class Texture> Swapchain::GetCurrentImage() const
{
    return m_ColorAttachments[m_CurrentImageIndex];
}

Framebuffer& Swapchain::GetCurrentFramebuffer()
{
    return m_Framebuffers[m_CurrentImageIndex];
}

void Swapchain::BeginFrame()
{
    auto device = m_Context->GetDevice();

    VK_CHECK(device.waitForFences(m_AcquireFence, VK_TRUE, UINT64_MAX));
    device.resetFences(m_AcquireFence);
    auto result = device.acquireNextImageKHR(m_Swapchain, UINT64_MAX, m_Semaphores.ImageAvailable, m_AcquireFence);
    m_CurrentImageIndex = result.value;

    if (result.result == vk::Result::eErrorOutOfDateKHR)
        CreateSwapchain();
    else if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR)
        VK_CHECK(result.result);
}

bool Swapchain::Present()
{
    auto presentQueue = m_Context->GetQueue(EQueueFamilyType::Present);

    const auto presentInfo = vk::PresentInfoKHR(
        1,
        &m_Semaphores.RenderFinished,
        1,
        &m_Swapchain,
        &m_CurrentImageIndex
    );
    vk::Result result;

    try
    {
        result = presentQueue.presentKHR(presentInfo);
        m_FrameIndex = (m_FrameIndex + 1) % m_ColorAttachments.size();
        // TODO: this is a temporary solution, m_CurrentFrameIndex should be current frame in flight not just the current frame index
        m_Context->m_CurrentFrameInFlight = (m_Context->m_CurrentFrameInFlight + 1) % m_Context->m_MaxFramesInFlight;
        return true;
    }
    catch (vk::SystemError& error)
    {
        if (error.code() == vk::Result::eErrorOutOfDateKHR || error.code() == vk::Result::eSuboptimalKHR)
            return false;
        LNE_ASSERT(false, "Failed to present swapchain image: {}", error.what());
        return false;
    }
}

void Swapchain::CreateSwapchain()
{
    auto device = m_Context->GetDevice();
    m_Context->WaitIdle();

    auto sc = m_Context->GetSurfaceCapabilities(m_Surface);
    vk::SurfaceFormatKHR surfaceFormat = PickSwapchainSurfaceFormat(m_Context->GetSurfaceFormats(m_Surface));
    vk::PresentModeKHR presentMode = PickSwapchainPresentMode(m_Context->GetSurfacePresentModes(m_Surface));

    m_SurfaceFormat = surfaceFormat;

    vk::SurfaceTransformFlagBitsKHR preTransform = (sc.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
        : sc.currentTransform;

    const uint32_t imageCount = std::clamp(sc.minImageCount + 1, sc.minImageCount, sc.maxImageCount);
    const auto presentationFamilyIndex = m_Context->GetQueueFamilyIndices().PresentFamily;
    const auto graphicsFamilyIndex = m_Context->GetQueueFamilyIndices().GraphicsFamily;

    const bool sameQueueFamily = presentationFamilyIndex.value() == graphicsFamilyIndex.value();

    std::vector<uint32_t> queueFamilyIndices = sameQueueFamily ?
        std::vector<uint32_t>{} : std::vector<uint32_t>{ graphicsFamilyIndex.value(), presentationFamilyIndex.value() };

    auto oldSwapchain = m_Swapchain;

    vk::SwapchainCreateInfoKHR createInfo(
        {},
        m_Surface,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        sc.currentExtent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        sameQueueFamily ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
        queueFamilyIndices,
        preTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        presentMode,
        vk::True,
        oldSwapchain
    );
    
    if (sc.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
    {
        createInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    if (sc.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)
    {
        createInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    m_Swapchain = device.createSwapchainKHR(createInfo);
    m_Context->SetVkObjectName(m_Swapchain, "Swapchain");
    m_Viewport = Viewport(sc.currentExtent);

    if (oldSwapchain)
        device.destroySwapchainKHR(oldSwapchain);

    m_Framebuffers.clear();
    m_ColorAttachments.clear();

    auto images = device.getSwapchainImagesKHR(m_Swapchain);
    m_ColorAttachments.resize(images.size());
    m_Framebuffers.reserve(images.size());
    m_DepthAttachment = Texture::CreateDepthTexture(m_Context, sc.currentExtent.width, sc.currentExtent.height, "SwapchainDepth");

    AttachmentDesc colorAttachmentDesc{
        .LoadOp = vk::AttachmentLoadOp::eClear,
        .StoreOp = vk::AttachmentStoreOp::eStore,
        .InitialLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .FinalLayout = vk::ImageLayout::ePresentSrcKHR,
        .ClearValue = vk::ClearColorValue{0.105f, 0.117f, 0.149f, 0.1f},
    };

    AttachmentDesc depthAttachmentDesc{
        .LoadOp = vk::AttachmentLoadOp::eClear,
        .StoreOp = vk::AttachmentStoreOp::eDontCare,
        .InitialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .FinalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .ClearValue = vk::ClearDepthStencilValue{1.0f, 0},
    };

    for (uint32_t i = 0; i < images.size(); ++i)
    {
        m_Context->SetVkObjectName(images[i], std::format("Image: Swapchain {}", i));
        m_ColorAttachments[i].Reset(lnnew Texture(m_Context, images[i], surfaceFormat.format, vk::Extent3D(sc.currentExtent, 1), 1, std::format("SwapchainColor{}", i)));
        colorAttachmentDesc.Texture = m_ColorAttachments[i];
        depthAttachmentDesc.Texture = m_DepthAttachment;
        m_Framebuffers.emplace_back(Framebuffer(m_Context, { colorAttachmentDesc }, depthAttachmentDesc));
    }
    m_Context->m_MaxFramesInFlight = (uint32_t)m_ColorAttachments.size() - 1;
}

void Swapchain::CreateSyncObjects()
{
    auto device = m_Context->GetDevice();

    vk::FenceCreateInfo fenceCI{ vk::FenceCreateFlagBits::eSignaled };
    m_AcquireFence = device.createFence(fenceCI);

    vk::SemaphoreCreateInfo semaphoreCI{};

    m_Semaphores = SwapchainSemaphores{
        .ImageAvailable = device.createSemaphore(semaphoreCI),
        .RenderFinished = device.createSemaphore(semaphoreCI)
    };

    m_Context->SetVkObjectName(m_Semaphores.ImageAvailable, "Swapchain Semaphore ImageAvailable");
    m_Context->SetVkObjectName(m_Semaphores.RenderFinished, "Swapchain Semaphore RenderFinished");
}

vk::SurfaceFormatKHR Swapchain::PickSwapchainSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR Swapchain::PickSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            LNE_INFO("Present mode: Mailbox");
            return availablePresentMode;
        }
    }

    LNE_INFO("Present mode: V-Sync");
    return vk::PresentModeKHR::eFifo;
}

}
