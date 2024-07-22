#include "SwapChain.h"
#include "GfxContext.h"

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

    vk::Device vkDevice = device->GetHandle();

    vkDevice.destroySemaphore(m_Semaphores.ImageAvailable);
    vkDevice.destroySemaphore(m_Semaphores.RenderFinished);
    for (auto& fence : m_WaitFences)
        vkDevice.destroyFence(fence);

    for (auto& image : m_Images)
        vkDevice.destroyImageView(image.ImageView);
    vkDevice.destroySwapchainKHR(m_Swapchain);
    m_Context->VulkanInstance().destroySurfaceKHR(m_Surface);
}

void Swapchain::Present()
{
    auto presentQueue = m_Context->GetDevice()->GetPresentQueue();

    const auto presentInfo = vk::PresentInfoKHR(
        1,
        &m_Semaphores.RenderFinished,
        1,
        &m_Swapchain,
        &m_CurrentImageIndex
    );

    presentQueue.presentKHR(presentInfo);
}

void Swapchain::CreateSwapchain()
{
    auto device = m_Context->GetDevice();
    auto vkDevice = device->GetHandle();
    auto physicalDevice = m_Context->GetPhysicalDevice();

    auto sc = physicalDevice->GetSurfaceCapabilities(m_Surface);
    vk::SurfaceFormatKHR surfaceFormat = PickSwapchainSurfaceFormat(physicalDevice->GetSurfaceFormats(m_Surface));
    vk::PresentModeKHR presentMode = PickSwapchainPresentMode(physicalDevice->GetSurfacePresentModes(m_Surface));

    vk::SurfaceTransformFlagBitsKHR preTransform = (sc.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
        : sc.currentTransform;

    const uint32_t imageCount = std::clamp(sc.minImageCount + 1, sc.minImageCount, sc.maxImageCount);
    const auto presentationFamilyIndex = device->GetQueueFamilyIndices().PresentFamily;
    const auto graphicsFamilyIndex = device->GetQueueFamilyIndices().GraphicsFamily;

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

    m_Swapchain = vkDevice.createSwapchainKHR(createInfo);
    m_Context->SetVkObjectName(m_Swapchain, vk::ObjectType::eSwapchainKHR, "Swapchain");

    if (oldSwapchain)
        vkDevice.destroySwapchainKHR(oldSwapchain);

    for (auto& image : m_Images)
        vkDevice.destroyImageView(image.ImageView);

    m_Images.clear();
    auto images = vkDevice.getSwapchainImagesKHR(m_Swapchain);
    m_Images.reserve(images.size());
    vk::ImageViewCreateInfo scImageViewCI(
        {},
        {},
        vk::ImageViewType::e2D,
        surfaceFormat.format,
        {},
        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );

    for (uint32_t i = 0; i < images.size(); ++i)
    {
        m_Context->SetVkObjectName(images[i], vk::ObjectType::eImage, std::format("Swapchain Image: {}", i));
        scImageViewCI.image = images[i];
        m_Images.push_back({ images[i], vkDevice.createImageView(scImageViewCI, nullptr) });
        m_Context->SetVkObjectName(m_Images[i].ImageView, vk::ObjectType::eImageView, std::format("Swapchain ImageView: {}", i));
    }
}

void Swapchain::CreateSyncObjects()
{
    auto device = m_Context->GetDevice();
    auto vkDevice = device->GetHandle();

    vk::SemaphoreCreateInfo semaphoreCI{};
    vk::FenceCreateInfo fenceCI{ vk::FenceCreateFlagBits::eSignaled };

    m_Semaphores.ImageAvailable = vkDevice.createSemaphore(semaphoreCI);
    m_Context->SetVkObjectName(m_Semaphores.ImageAvailable, vk::ObjectType::eSemaphore, "Swapchain Semaphore ImageAvailable");
    m_Semaphores.RenderFinished = vkDevice.createSemaphore(semaphoreCI);
    m_Context->SetVkObjectName(m_Semaphores.RenderFinished, vk::ObjectType::eSemaphore, "Swapchain Semaphore RenderFinished");

    m_WaitFences.resize(m_Images.size());
    for (auto& fence : m_WaitFences)
    {
        fence = vkDevice.createFence(fenceCI);
        m_Context->SetVkObjectName(fence, vk::ObjectType::eFence, "Swapchain Fence");
    }
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
