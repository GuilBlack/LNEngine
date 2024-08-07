#pragma once
#include "GfxEnums.h"
#include "Engine/Core/SafePtr.h"

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

    void BeginRenderPass(const class Framebuffer& framebuffer) const;
    void EndRenderPass(const class Framebuffer& framebuffer) const;

    void Draw(SafePtr<class GraphicsPipeline> pipeline);

    SafePtr<class GraphicsPipeline> CreateGraphicsPipeline(const struct GraphicsPipelineDesc& createInfo);

private:
    SafePtr<class GfxContext> m_Context;
    SafePtr<class Swapchain> m_Swapchain;
    std::unique_ptr<class CommandBufferManager> m_GraphicsCommandBufferManager;
};
}
