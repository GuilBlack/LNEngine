#include "SwapChain.h"
#include "GfxContext.h"
#include "Texture.h"
#include "Framebuffer.h"

namespace lne
{
Swapchain::Swapchain(std::shared_ptr<class GfxContext> ctx, vk::SurfaceKHR surface)
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

    m_Images.clear();

    device.destroySwapchainKHR(m_Swapchain);
    m_Context->VulkanInstance().destroySurfaceKHR(m_Surface);
}

vk::SubmitInfo Swapchain::GetSubmitInfo(const vk::CommandBuffer* cmdBuffer, vk::PipelineStageFlags submitStageFlag, bool waitForImageAvailable, bool signalRenderFinished) const
{
    // TODO: use the submitStageFlag
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    vk::SubmitInfo submitInfo(
        waitForImageAvailable ? 1 : 0,
        waitForImageAvailable ? &m_Semaphores.ImageAvailable : nullptr,
        waitForImageAvailable ? waitStages : nullptr,
        1,
        cmdBuffer,
        signalRenderFinished ? 1 : 0,
        signalRenderFinished ? &m_Semaphores.RenderFinished : nullptr
    );

    return submitInfo;
}

std::shared_ptr<class Texture> Swapchain::GetCurrentImage() const
{
    return m_Images[m_CurrentFrameIndex];
}

void Swapchain::BeginFrame()
{
    auto device = m_Context->GetDevice();
    auto result = device.acquireNextImageKHR(m_Swapchain, UINT64_MAX, m_Semaphores.ImageAvailable);
    m_CurrentFrameIndex = result.value;

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
        &m_CurrentFrameIndex
    );

    vk::Result result = presentQueue.presentKHR(presentInfo);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
        return false;

    return true;
}

void Swapchain::CreateSwapchain()
{
    auto device = m_Context->GetDevice();
    m_Context->WaitIdle();

    auto sc = m_Context->GetSurfaceCapabilities(m_Surface);
    vk::SurfaceFormatKHR surfaceFormat = PickSwapchainSurfaceFormat(m_Context->GetSurfaceFormats(m_Surface));
    vk::PresentModeKHR presentMode = PickSwapchainPresentMode(m_Context->GetSurfacePresentModes(m_Surface));

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
    m_Context->SetVkObjectName(m_Swapchain, vk::ObjectType::eSwapchainKHR, "Swapchain");

    if (oldSwapchain)
        device.destroySwapchainKHR(oldSwapchain);

    m_Framebuffers.clear();
    m_Images.clear();

    auto images = device.getSwapchainImagesKHR(m_Swapchain);
    m_Images.resize(images.size());
    m_Framebuffers.reserve(images.size());

    for (uint32_t i = 0; i < images.size(); ++i)
    {
        m_Context->SetVkObjectName(images[i], vk::ObjectType::eImage, std::format("Image: Swapchain {}", i));
        m_Images[i].reset(new Texture(m_Context, images[i], surfaceFormat.format, vk::Extent3D(sc.currentExtent, 1), 1, std::format("Swapchain {}", i)));
    }
}

void Swapchain::CreateSyncObjects()
{
    auto device = m_Context->GetDevice();

    vk::SemaphoreCreateInfo semaphoreCI{};

    m_Semaphores.ImageAvailable = device.createSemaphore(semaphoreCI);
    m_Context->SetVkObjectName(m_Semaphores.ImageAvailable, vk::ObjectType::eSemaphore, "Swapchain Semaphore ImageAvailable");
    m_Semaphores.RenderFinished = device.createSemaphore(semaphoreCI);
    m_Context->SetVkObjectName(m_Semaphores.RenderFinished, vk::ObjectType::eSemaphore, "Swapchain Semaphore RenderFinished");
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
            std::cout << "Present mode: Mailbox" << std::endl;
            return availablePresentMode;
        }
    }

    std::cout << "Present mode: V-Sync" << std::endl;
    return vk::PresentModeKHR::eFifo;
}

}
