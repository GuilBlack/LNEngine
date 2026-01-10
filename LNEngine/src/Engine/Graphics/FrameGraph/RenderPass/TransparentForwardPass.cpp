#include "TransparentForwardPass.h"

#include <Core/ApplicationBase.h>
#include <Graphics/Renderer.h>
#include <Graphics/WorldRenderer.h>
#include <Graphics/Resources/Mesh.h>
#include <Graphics/Resources/Material.h>

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
    uint32_t frameIndex = renderer.GetCurrentFrameIndex();
    auto lightBuffer = worldRenderer->GetLightBufferGPU(frameIndex);
    const TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(frameIndex);

    for (auto& [hash, drawCommand] : m_DrawCommands[frameIndex])
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        const SubMeshTransformArray& transforms = worldRenderer->GetTransforms(frameIndex, hash);
        renderer.Draw(commandBuffer, drawCommand.Mesh, transformBuffer.Buffer, lightBuffer, GetID(), transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}

void lne::TransparentForwardPass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex)
{
    const SubMesh& submesh = mesh->GetSubMeshes()[hash.SubMeshIndex];
    SafePtr material = mesh->GetMaterial(submesh.MaterialIndex);
    if (material->CanRenderToPass(GetID()) == false)
        return;
    uint32_t frameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    auto& drawCommands = m_DrawCommands[frameIndex][hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount++;
}
}
