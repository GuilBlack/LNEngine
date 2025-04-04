#pragma once
#include <Engine/Graphics/FrameGraph/RenderPass/IRenderPass.h>

namespace lne
{
class LightingPass : public lne::IRenderPass
{
public:
    LightingPass()
    {
        m_Name = "LightingPass";
    }
    void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;
    void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;
    void PostExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) override;
    void OnImGuiRender() override;
    
private:
    SafePtr<class GfxPipeline> m_Pipeline{};
    SafePtr<class Material> m_Material{};
    
    SafePtr<class Texture> m_DebugTexture{};
    bool m_IsDebugOpen{false};
};
}
