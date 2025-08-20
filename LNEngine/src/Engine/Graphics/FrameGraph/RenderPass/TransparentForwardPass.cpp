#include "TransparentForwardPass.h"

#include <Core/ApplicationBase.h>
#include <Graphics/Renderer.h>
#include <Graphics/WorldRenderer.h>
#include <Graphics/Material.h>
#include <Graphics/Mesh.h>

namespace lne
{
void TransparentForwardPass::OnBind(FrameGraph* frameGraph, FrameGraphNode* node)
{
}

void TransparentForwardPass::BeginFrame()
{
    ClearDrawCommands();
}

void TransparentForwardPass::Execute(vk::CommandBuffer commandBuffer, WorldRenderer* worldRenderer, FrameGraph* frameGraph,
    FrameGraphNode* node)
{
    auto& renderer = ApplicationBase::GetRenderer();
    for (auto& [hash, drawCommand] : m_DrawCommands)
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        SubMeshTransformArray& transforms = worldRenderer->GetTransforms(hash);
        TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(renderer.GetCurrentFrameIndex());
        renderer.Draw(commandBuffer, drawCommand.Mesh, transformBuffer.Buffer, transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}

void lne::TransparentForwardPass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex)
{
    const SubMesh& submesh = mesh->GetSubMeshes()[hash.SubMeshIndex];
    SafePtr material = mesh->GetMaterial(submesh.MaterialIndex);
    if (material->IsTransparent() == false)
        return;
    auto& drawCommands = m_DrawCommands[hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount++;
}
}
