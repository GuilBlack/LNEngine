#pragma once
#include "IRenderPass.h"

namespace lne
{
class DepthPrePass : public lne::IRenderPass, public lne::IDrawStaticMeshes
{
public:
    DepthPrePass();

    virtual void BeginFrame() override;

    virtual void Execute(vk::CommandBuffer cmdBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    virtual void OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

private:
    lne::SafePtr<lne::GfxPipeline> m_Pipeline{};
    lne::SafePtr<lne::Material> m_Material{};
};
}

