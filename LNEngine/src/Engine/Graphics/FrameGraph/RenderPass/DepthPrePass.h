#pragma once
#include "RenderPass.h"
#include "Interfaces/IDrawStaticMeshesAdder.h"

namespace lne
{
class DepthPrePass : public RenderPass, public IDrawStaticMeshesAdder
{
public:
    DepthPrePass();

    virtual void BeginFrame() override;

    virtual void Execute(CommandBuffer* cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

    void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex, u32 instanceCount) override;
};
}

