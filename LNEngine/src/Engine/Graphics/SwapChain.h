#pragma once
#include "GfxEnums.h"

namespace lne
{
class Swapchain
{
public:
    /// <summary>
    /// ctor for Swapchain
    /// </summary>
    /// <param name="ctx">keeps a ref to the graphics context so that it can handle its own resources internally</param>
    /// <param name="surface">the swapchain will own the surface</param>
    Swapchain(std::shared_ptr<class GfxContext> ctx, vk::SurfaceKHR surface);
    ~Swapchain();

    void CreateSwapchain();

    [[nodiscard]] uint32_t GetImageCount() const { return static_cast<uint32_t>(m_Images.size()); }
    [[nodiscard]] uint32_t GetCurrentFrameIndex() const { return m_CurrentImageIndex; }
    [[nodiscard]] vk::SubmitInfo GetSubmitInfo(const vk::CommandBuffer* cmdBuffer, 
        vk::PipelineStageFlags* submitStageFlag, 
        bool waitForImageAvailable = true, bool signalRenderFinished = true) const;
    [[nodiscard]] std::shared_ptr<class Texture> GetCurrentImage() const;

    [[nodiscard]] class Framebuffer& GetCurrentFramebuffer();

    void BeginFrame();
    [[nodiscard]] bool Present();

private:
    std::shared_ptr<class GfxContext> m_Context;
    vk::SwapchainKHR m_Swapchain{};
    vk::SurfaceKHR m_Surface{};

    std::vector<std::shared_ptr<class Texture>> m_Images;
    std::vector<class Framebuffer> m_Framebuffers;

    struct SwapchainSemaphores
    {
        vk::Semaphore ImageAvailable;
        vk::Semaphore RenderFinished;
    };
    SwapchainSemaphores m_Semaphores;
    vk::Fence m_AcquireFence;

    uint32_t m_CurrentImageIndex{ 0 };
    uint32_t m_FrameIndex{ 0 };
private:
    void CreateSyncObjects();

    vk::SurfaceFormatKHR PickSwapchainSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR PickSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
};
}
