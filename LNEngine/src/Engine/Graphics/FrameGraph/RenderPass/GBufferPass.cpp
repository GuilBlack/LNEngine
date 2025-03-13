#include "GBufferPass.h"
#include "Core/ApplicationBase.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/Renderer.h"
#include "Graphics/FrameGraph/FrameGraph.h"
#include "Graphics/Pipeline.h"
#include "Graphics/Material.h"

namespace lne
{
void GBufferPass::OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    Renderer& renderer = ApplicationBase::GetRenderer();
    GraphicsPipelineDesc desc{};
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\GBuffer.glsl";
    desc.Name = "GBufferPassShader";
    desc.EnableDepthTest(true, false);
    desc.Blend.EnableBlend(false);
    desc.CullMode = ECullMode::Back;
    desc.FrameGraph = frameGraph;

    m_Pipeline = renderer.CreateGraphicsPipeline(desc);
    m_Material = lnnew Material(m_Pipeline);
}

void GBufferPass::BeginFrame()
{
    ClearDrawCommands();
}

void GBufferPass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    auto& renderer = ApplicationBase::GetRenderer();
    for (auto& [hash, drawCommand] : m_DrawCommands)
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        SubMeshTransformArray& transforms = worldRenderer->GetTransforms(hash);
        TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(renderer.GetCurrentFrameIndex());

        renderer.Draw(cmdBuffer, drawCommand.Mesh, transformBuffer.Buffer, m_Material, transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}
}
