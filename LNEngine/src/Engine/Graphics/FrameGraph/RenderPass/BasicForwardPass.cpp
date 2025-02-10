#include "BasicForwardPass.h"

#include <Core/ApplicationBase.h>
#include <Graphics/Renderer.h>
#include <Graphics/WorldRenderer.h>

namespace lne
{
void BasicForwardPass::OnBind(FrameGraph* frameGraph, FrameGraphNode* node)
{
    Renderer& renderer = ApplicationBase::GetRenderer();
    GraphicsPipelineDesc desc{};
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\Skybox.glsl";
    desc.Name = "Skybox";
    desc.FrameGraph = frameGraph;
    desc.EnableDepthTest(true, false);
    desc.Blend.EnableBlend(false);
    desc.CullMode = ECullMode::None;
    m_SkyboxPipeline = renderer.CreateGraphicsPipeline(desc);
    m_SkyboxMaterial = lnnew Material(m_SkyboxPipeline);

    std::string cubemapPath = lne::ApplicationBase::GetAssetsPath() + "Textures\\Skybox\\";
    m_SkyboxTexture = renderer.CreateCubemapTexture({
        cubemapPath + "px.png",
        cubemapPath + "nx.png",
        cubemapPath + "py.png",
        cubemapPath + "ny.png",
        cubemapPath + "pz.png",
        cubemapPath + "nz.png"
    });

    m_SkyboxMaterial->SetTexture("tAlbedo", m_SkyboxTexture);
    m_SkyboxMesh = lnnew StaticMesh(Geometry::GenerateUVSphere(1.f, 8, 8), m_SkyboxMaterial, { m_SkyboxTexture }, m_SkyboxPipeline);
}

void BasicForwardPass::BeginFrame()
{
    ClearDrawCommands();
}

void BasicForwardPass::Execute(vk::CommandBuffer commandBuffer, WorldRenderer* worldRenderer, FrameGraph* frameGraph,
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
    renderer.Draw(commandBuffer, m_SkyboxMesh, worldRenderer->GetTransformBuffer(renderer.GetCurrentFrameIndex()).Buffer, 0, 0, 1);
}
}
