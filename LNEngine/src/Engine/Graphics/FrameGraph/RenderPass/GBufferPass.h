#pragma once
#include <Engine/Graphics/FrameGraph/RenderPass/IRenderPass.h>

namespace lne
{
class GBufferPass : public lne::IRenderPass, public IDrawStaticMeshes
{
public:
    GBufferPass()
    {
        m_Name = "GBufferPass";
    }

    void OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void BeginFrame() override;

    void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void OnImGuiRender() override;

private:
    SafePtr<class GfxPipeline> m_Pipeline{};
    SafePtr<class Material> m_Material{};
    std::unordered_map<std::string, SafePtr<class Texture>> m_DebugTextures{};
};
}

