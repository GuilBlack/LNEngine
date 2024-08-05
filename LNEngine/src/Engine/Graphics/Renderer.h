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

    void BeginRenderPass(const class Framebuffer& framebuffer) const;
    void EndRenderPass(const class Framebuffer& framebuffer) const;

    void Draw(std::shared_ptr<class GraphicsPipeline> pipeline);

    std::shared_ptr<class GraphicsPipeline> CreateGraphicsPipeline(const struct GraphicsPipelineDesc& createInfo);

private:
    std::shared_ptr<class GfxContext> m_Context;
    std::shared_ptr<class Swapchain> m_Swapchain;
    std::unique_ptr<class CommandBufferManager> m_GraphicsCommandBufferManager;
};
}
