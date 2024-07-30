#pragma once
#include "GfxEnums.h"

namespace lne
{
class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void Init(std::unique_ptr<class Window>& window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void BeginRenderPass(class Framebuffer& framebuffer);
    void EndRenderPass(class Framebuffer& framebuffer);

private:
    std::shared_ptr<class GfxContext> m_Context;
    std::shared_ptr<class Swapchain> m_Swapchain;
    std::unique_ptr<class CommandBufferManager> m_GraphicsCommandBufferManager;
};
}
