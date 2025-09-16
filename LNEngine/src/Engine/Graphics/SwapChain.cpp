#include "SwapChain.h"
#include "GfxContext.h"
#include "Resources/Texture.h"
#include "Framebuffer.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Log.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Renderer.h"

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
    uint32_t framesInFlight = m_Context->GetMaxFramesInFlight();
    for (uint32_t i = 0; i < framesInFlight; i++)
    {
        device.destroySemaphore(m_Semaphores[i].ImageAvailable);
        device.destroySemaphore(m_Semaphores[i].RenderFinished);
        device.destroyFence(m_AcquireFences[i]);
    }

    m_ColorAttachments.clear();

    device.destroySwapchainKHR(m_Swapchain);
    m_Context->VulkanInstance().destroySurfaceKHR(m_Surface);
}

vk::SubmitInfo Swapchain::GetSubmitInfo(vk::PipelineStageFlags* waitStages, uint32_t frameInFlight) const
{
    vk::SubmitInfo submitInfo(
        1,
        &m_Semaphores[frameInFlight].ImageAvailable,
        waitStages,
        1,
        {},
        1,
        &m_Semaphores[frameInFlight].RenderFinished
    );

    return submitInfo;
}

SafePtr<class Texture> Swapchain::GetCurrentImage() const
{
    return m_ColorAttachments[m_CurrentImageIndex];
}

lne::SafePtr<class Texture> Swapchain::GetImage(uint32_t index) const
{
    LNE_ASSERT(index < m_ColorAttachments.size(), "Index out of bounds");
    return m_ColorAttachments[index];
}

class Framebuffer& Swapchain::GetFramebuffer(uint32_t index)
{
    LNE_ASSERT(index < m_Framebuffers.size(), "Index out of bounds");
    return m_Framebuffers[index];
}

void Swapchain::BeginFrame()
{
	auto device = m_Context->GetDevice();
	uint32_t currentFrameInFlight = m_Context->GetCurrentFrameIndex();
	VK_CHECK(device.waitForFences(m_AcquireFences[currentFrameInFlight], VK_TRUE, UINT64_MAX));
	device.resetFences(m_AcquireFences[currentFrameInFlight]);
	auto result = device.acquireNextImageKHR(m_Swapchain, UINT64_MAX, m_Semaphores[currentFrameInFlight].ImageAvailable, m_AcquireFences[currentFrameInFlight]);
	m_CurrentImageIndex = result.value;

	if (result.result == vk::Result::eErrorOutOfDateKHR)
		CreateSwapchain();
	else if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR)
		VK_CHECK(result.result);
}

bool Swapchain::Present()
{
    auto present = [this]() 
        {
            auto presentQueue = m_Context->GetQueue(EQueueFamilyType::Present);

            const auto presentInfo = vk::PresentInfoKHR(
                1,
                &m_Semaphores[m_Context->GetCurrentFrameIndex()].RenderFinished,
                1,
                &m_Swapchain,
                &m_CurrentImageIndex
            );
            vk::Result result;

            try
            {
                m_Context->m_CurrentFrameInFlight = (m_Context->m_CurrentFrameInFlight + 1) % m_Context->m_MaxFramesInFlight;
                result = presentQueue.presentKHR(presentInfo);
                m_FrameIndex = (m_FrameIndex + 1) % m_ColorAttachments.size();
                // TODO: this is a temporary solution, m_CurrentFrameIndex should be current frame in flight not just the current frame index
                return true;
            }
            catch (vk::SystemError& error)
            {
                if (error.code() == vk::Result::eErrorOutOfDateKHR || error.code() == vk::Result::eSuboptimalKHR)
                    return false;
                LNE_ASSERT(false, "Failed to present swapchain image: {}", error.what());
                return false;
            }
        };
    auto& renderer = ApplicationBase::GetRenderer();
    if (renderer.IsAsync())
        renderer.AddRenderTask(present);
    else
        present();
    return true;
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
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
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
    m_DepthAttachment = Texture::CreateDepthTexture(m_Context, 
        sc.currentExtent.width, sc.currentExtent.height, 
        TextureUsageType::eSampled, "SwapchainDepth");

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
    uint32_t count = m_Context->GetMaxFramesInFlight();

    vk::FenceCreateInfo fenceCI{ vk::FenceCreateFlagBits::eSignaled };
    m_AcquireFences.clear();
    m_AcquireFences.reserve(count);

    vk::SemaphoreCreateInfo semaphoreCI{};
    m_Semaphores.clear();
    m_Semaphores.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        m_AcquireFences.push_back(device.createFence(fenceCI));
        m_Context->SetVkObjectName(m_AcquireFences[i], std::format("Swapchain Acquire Fence {}", i));

        m_Semaphores.push_back(SwapchainSemaphores{
            .ImageAvailable = device.createSemaphore(semaphoreCI),
            .RenderFinished = device.createSemaphore(semaphoreCI)
            });
        m_Context->SetVkObjectName(m_Semaphores[i].ImageAvailable, std::format("Swapchain Semaphore ImageAvailable {}", i));
        m_Context->SetVkObjectName(m_Semaphores[i].RenderFinished, std::format("Swapchain Semaphore RenderFinished {}", i));
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
            LNE_INFO("Present mode: Mailbox");
            return availablePresentMode;
        }
    }

    LNE_INFO("Present mode: V-Sync");
    return vk::PresentModeKHR::eFifo;
}

}
