#pragma once
#include "IRenderPass.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{
class Material;

class MeshletDebugPass : public IRenderPass, public IDrawStaticMeshes
{
public:
    MeshletDebugPass();

    void BeginFrame() override;

    void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;

    void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void OnImGuiRender() override;

    void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex) override;

private:
    SafePtr<Material> m_Material;
    FrameGraphNode* m_AttachedNodeRef;
};
}
