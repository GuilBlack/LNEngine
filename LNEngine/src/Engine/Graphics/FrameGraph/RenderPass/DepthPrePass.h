#pragma once
#include "RenderPass.h"

namespace lne
{
class DepthPrePass : public RenderPass, public IDrawStaticMeshes
{
public:
    DepthPrePass();

    virtual void BeginFrame() override;

    virtual void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

    void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex) override;
};
}

