#pragma once
#include "IRenderPass.h"

namespace lne
{
class DepthPrePass : public IRenderPass, public IDrawStaticMeshes
{
public:
    DepthPrePass();

    virtual void BeginFrame() override;

    virtual void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

    virtual void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;

private:
    SafePtr<class GfxPipeline> m_Pipeline{};
    SafePtr<class Material> m_Material{};
};
}

