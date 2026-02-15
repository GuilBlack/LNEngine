#include "lnepch.h"
#include "DepthPrePass.h"
#include "Core/ApplicationBase.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/Renderer.h"
#include "Core/Utils/Profiling.h"
#include "Graphics/Resources/Mesh.h"
#include <Graphics/Resources/Pipeline.h>
#include "Graphics/Resources/Material.h"

namespace lne
{
DepthPrePass::DepthPrePass()
{
    m_Name = "DepthPrePass";
}

void DepthPrePass::BeginFrame()
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    ClearDrawCommands();
}

void DepthPrePass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    using namespace lne;
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
        renderer.Draw(cmdBuffer, drawArgs);
    }
}

void DepthPrePass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex)
{
    const SubMesh& submesh = mesh->GetSubMeshes()[hash.SubMeshIndex];
    SafePtr material = mesh->GetMaterial(submesh.MaterialIndex);
    if (material->CanRenderToPass(GetID()) == false)
        return;
    u32 frameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    auto& drawCommands = m_DrawCommands[frameIndex][hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount++;
}

}