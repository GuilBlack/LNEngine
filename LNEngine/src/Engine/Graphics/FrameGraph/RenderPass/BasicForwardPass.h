#pragma once
#include "IRenderPass.h"

namespace lne
{
class BasicForwardPass : public IRenderPass, public IDrawStaticMeshes
{
public:
    BasicForwardPass()
    {
        m_Name = "BasicForwardPass";
    }
    virtual void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;
    virtual void BeginFrame() override;
    virtual void Execute(vk::CommandBuffer commandBuffer, WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

public:
    SafePtr<GfxPipeline> m_SkyboxPipeline{};
    SafePtr<Material> m_SkyboxMaterial{};
    SafePtr<StaticMesh> m_SkyboxMesh{};
    SafePtr<Texture> m_SkyboxTexture{};
};
}
