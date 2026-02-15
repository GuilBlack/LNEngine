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
    u32 frameIndex = renderer.GetCurrentFrameIndex();
    auto lightBuffer = worldRenderer->GetLightBufferGPU(frameIndex);
    const TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(frameIndex);
    DrawMeshArgs drawArgs{
        .TransformBuffer = transformBuffer.Buffer,
        .LightsBuffer = lightBuffer,
        .PassId = GetID(),
    };

    for (auto& [hash, drawCommand] : m_DrawCommands[frameIndex])
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        const SubMeshTransformArray& transforms = worldRenderer->GetTransforms(frameIndex, hash);

        drawArgs.Mesh = mesh;
        drawArgs.Offset = transforms.Offset;
        drawArgs.SubMeshIndex = drawCommand.SubMeshIndex;
        drawArgs.InstanceCount = drawCommand.InstanceCount;
        renderer.Draw(commandBuffer, drawArgs);
    }
}

void lne::TransparentForwardPass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex, u32 instanceCount)
{
    const SubMesh& submesh = mesh->GetSubMeshes()[hash.SubMeshIndex];
    SafePtr material = mesh->GetMaterial(submesh.MaterialIndex);
    if (material->CanRenderToPass(GetID()) == false)
        return;
    u32 frameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    auto& drawCommands = m_DrawCommands[frameIndex][hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount = instanceCount;
}
}
