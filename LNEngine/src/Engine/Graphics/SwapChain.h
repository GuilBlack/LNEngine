#pragma once
#include "GfxEnums.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{

struct Viewport
{
    Viewport(const vk::Extent2D& extent) : m_Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f) {}
    Viewport() = default;
    Viewport(const Viewport&) = default;
    Viewport& operator=(const Viewport&) = default;
    Viewport(const vk::Viewport& vp) : m_Viewport(vp) {}

    Viewport& operator=(const vk::Viewport& vp) { m_Viewport = vp; return *this; }
    Viewport& operator=(const vk::Extent2D& extent)
    {
        m_Viewport = vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f); return *this;
    }

    vk::Extent2D GetExtent() const { return vk::Extent2D(static_cast<uint32_t>(m_Viewport.width), static_cast<uint32_t>(m_Viewport.height)); }

    const vk::Viewport& GetViewport() const { return m_Viewport; }
    const vk::Rect2D GetScissor() const { return vk::Rect2D({ 0, 0 }, GetExtent()); }
    
private:
    vk::Viewport m_Viewport;
};
class Swapchain : public RefCountBase
{
public:
    /// <summary>
    /// ctor for Swapchain
    /// </summary>
    /// <param name="ctx">keeps a ref to the graphics context so that it can handle its own resources internally</param>
    /// <param name="surface">the swapchain will own the surface</param>
    Swapchain(SafePtr<class GfxContext> ctx, vk::SurfaceKHR surface);
    virtual ~Swapchain();

    void CreateSwapchain();

    [[nodiscard]] uint32_t GetImageCount() const { return static_cast<uint32_t>(m_Images.size()); }
    [[nodiscard]] uint32_t GetCurrentFrameIndex() const { return m_CurrentImageIndex; }
    [[nodiscard]] vk::SubmitInfo GetSubmitInfo(vk::PipelineStageFlags* submitStageFlag, 
        bool waitForImageAvailable = true, bool signalRenderFinished = true) const;
    [[nodiscard]] SafePtr<class Texture> GetCurrentImage() const;
    [[nodiscard]] const Viewport& GetViewport() const { return m_Viewport; }

    [[nodiscard]] class Framebuffer& GetCurrentFramebuffer();

    void BeginFrame();
    [[nodiscard]] bool Present();

private:
    SafePtr<class GfxContext> m_Context;
    vk::SwapchainKHR m_Swapchain{};
    vk::SurfaceKHR m_Surface{};
    Viewport m_Viewport;

    std::vector<SafePtr<class Texture>> m_Images;
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
