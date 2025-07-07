#include "lnepch.h"
#include "DepthPrePass.h"
#include "Core/ApplicationBase.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/Renderer.h"
#include "Core/Utils/Profiling.h"

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
    for (auto& [hash, drawCommand] : m_DrawCommands)
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        SubMeshTransformArray& transforms = worldRenderer->GetTransforms(hash);
        TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(renderer.GetCurrentFrameIndex());
        renderer.Draw(cmdBuffer, drawCommand.Mesh, transformBuffer.Buffer, m_Material, transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}

void DepthPrePass::OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    Renderer& renderer = ApplicationBase::GetRenderer();z
    GraphicsPipelineDesc desc{};
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\DepthPrePass.glsl";
    desc.Name = "DepthPrePassShader";
    desc.EnableDepthTest(true, true);
    desc.Blend.EnableBlend(false);
    desc.CullMode = ECullMode::Back;
    desc.FrameGraph = frameGraph;

    m_Pipeline = renderer.CreateGraphicsPipeline(desc);
    m_Material = lnnew Material(m_Pipeline);
}

}