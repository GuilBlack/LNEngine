#include "GBufferPass.h"
#include "Core/ApplicationBase.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/Renderer.h"
#include "Graphics/FrameGraph/FrameGraph.h"
#include "Graphics/Pipeline.h"
#include "Graphics/Material.h"
#include "Graphics/GfxContext.h"

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
    SafePtr<GfxContext> graphicsContext = ApplicationBase::GetWindow().GetGfxContext();
    m_Pipeline = renderer.CreateGraphicsPipeline(desc);
    m_Material = lnnew Material(m_Pipeline);

    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = Texture::CreateColorTexture2D(graphicsContext,
                texture->GetDimensions().width / 2, texture->GetDimensions().height / 2, texture->GetFormat(), TextureUsageType::eSampled, false, texture->GetName() + "ImGUI Debug");
            m_DebugTextures.emplace(texture->GetName(), debugTexture);
        }
    }
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

void GBufferPass::PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    Renderer& renderer = ApplicationBase::GetRenderer();

    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = m_DebugTextures[texture->GetName()];
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderer.Blit(cmdBuffer, texture, debugTexture);
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}

void GBufferPass::OnImGuiRender()
{
    for (auto& [name, texture] : m_DebugTextures)
    {
        float textureRatio = (float)texture->GetDimensions().width / (float)texture->GetDimensions().height;
        float windowWidth = ImGui::GetWindowWidth();
        if (ImGui::TreeNode(name.c_str()))
        {
            ImGui::Image((ImTextureID)(uint64_t)texture->GetBindlessTextureHandle(), ImVec2(windowWidth, windowWidth / textureRatio));
            ImGui::TreePop();
        }
    }
}
}
