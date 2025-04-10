#pragma once
#include "IRenderPass.h"

namespace lne
{
class TransparentForwardPass : public IRenderPass, public IDrawStaticMeshes
{
public:
    TransparentForwardPass()
    {
        m_Name = "TransparentForwardPass";
    }
    virtual void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;
    virtual void BeginFrame() override;
    virtual void Execute(vk::CommandBuffer commandBuffer, WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

    virtual void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex) override;
};
}
