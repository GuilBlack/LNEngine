#include "MeshletDebugPass.h"

#include "Core/ApplicationBase.h"
#include "Core/Window.h"
#include "Core/Utils/Profiling.h"

#include "Graphics/Renderer.h"
#include "Graphics/GfxContext.h"
#include "Graphics/WorldRenderer.h"

#include "Graphics/Resources/Mesh.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/GfxTechnique.h"
#include "Graphics/Resources/Effect.h"
#include "Graphics/FrameGraph/FrameGraph.h"

lne::MeshletDebugPass::MeshletDebugPass()
{
    m_Name = "MeshletDebugPass";
}

void lne::MeshletDebugPass::BeginFrame()
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    ClearDrawCommands();
}

void lne::MeshletDebugPass::OnBind(FrameGraph* frameGraph, FrameGraphNode* node)
{
    auto& renderer = ApplicationBase::GetRenderer();
    SafePtr<Effect> meshletDebugEffect = renderer.CreateOrGetEffect(ApplicationBase::GetAssetsPath() +
                                                                    "Engine\\Shaders\\Meshlet\\MeshletDebug.glsl");

    GfxTechniqueDesc meshletDebugTechDesc{};
    meshletDebugTechDesc.Name = "MeshletDebugOpaque";
    meshletDebugTechDesc.TechniqueState.Cull = ECullMode::Back;
    meshletDebugTechDesc.TechniqueState.Fill = EFillMode::Solid;
    meshletDebugTechDesc.TechniqueState.Transparency = TransparencyMode::eOpaque;
    meshletDebugTechDesc.TechniqueState.DepthMode = DepthMode::eReadWrite;
    meshletDebugTechDesc.TechniqueState.DepthCompareOp = ECompareOperation::GreaterOrEqual;
    PassBindingDesc meshletPassDesc{};
    meshletPassDesc.PassName = m_Name;
    meshletPassDesc.PassEffect = meshletDebugEffect;
    meshletDebugTechDesc.Passes.push_back(meshletPassDesc);
    SafePtr meshletTech = renderer.CreateOrGetTechnique(meshletDebugTechDesc);
    m_Material = lnnew Material(meshletTech);
    m_AttachedNodeRef = node;
}

void lne::MeshletDebugPass::Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    auto& renderer = ApplicationBase::GetRenderer();
    uint32_t frameIndex = renderer.GetCurrentFrameIndex();
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

        auto& submesh = mesh->GetSubMeshes()[drawCommand.SubMeshIndex];
        renderer.DrawMeshlets(cmdBuffer, drawArgs, m_Material);
    }
}

void lne::MeshletDebugPass::PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
}

void lne::MeshletDebugPass::OnImGuiRender()
{
    ImGui::Checkbox("Is Enabled", &m_AttachedNodeRef->Enabled);
}

void lne::MeshletDebugPass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex)
{
    if (mesh->GetGeometry().GetType() != GeometryType::eMeshlet)
        return;

    uint32_t frameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    auto& drawCommands = m_DrawCommands[frameIndex][hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount++;
}
