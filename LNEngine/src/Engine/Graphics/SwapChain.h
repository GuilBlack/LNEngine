#pragma once

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

    void Present();

    [[nodiscard]] uint32_t GetImageCount() const { return static_cast<uint32_t>(m_Images.size()); }

private:
    std::shared_ptr<class GfxContext> m_Context;
    vk::SwapchainKHR m_Swapchain{};
    vk::SurfaceKHR m_Surface{};

    struct SwapchainImage
    {
        vk::Image Image;
        vk::ImageView ImageView;
    };
    std::vector<SwapchainImage> m_Images;

    struct SwapchainSemaphores
    {
        vk::Semaphore ImageAvailable;
        vk::Semaphore RenderFinished;
    } m_Semaphores;

    std::vector<vk::Fence> m_WaitFences;

    uint32_t m_CurrentImageIndex{ 0 };
private:
    void CreateSwapchain();
    void CreateSyncObjects();

    vk::SurfaceFormatKHR PickSwapchainSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR PickSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
};
}
