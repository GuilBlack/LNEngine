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
    uint32_t frameIndex = renderer.GetCurrentFrameIndex();
    for (auto& [hash, drawCommand] : m_DrawCommands[frameIndex])
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        SubMeshTransformArray& transforms = worldRenderer->GetTransforms(frameIndex, hash);
        TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(frameIndex);
        renderer.Draw(cmdBuffer, drawCommand.Mesh, transformBuffer.Buffer, GetID(), transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}

void DepthPrePass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex)
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