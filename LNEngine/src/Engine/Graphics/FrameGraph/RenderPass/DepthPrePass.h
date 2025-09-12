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

    void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex) override;
};
}

