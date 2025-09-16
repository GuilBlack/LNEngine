#include "GBufferPass.h"
#include "Core/ApplicationBase.h"
#include "Core/Window.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/Renderer.h"
#include "Graphics/FrameGraph/FrameGraph.h"
#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/Mesh.h"
#include "Graphics/Resources/Texture.h"
#include "Graphics/GfxContext.h"
#include "Core/Utils/Profiling.h"

namespace lne
{
void GBufferPass::OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    Renderer& renderer = ApplicationBase::GetRenderer();
    SafePtr<GfxContext> graphicsContext = ApplicationBase::GetWindow().GetGfxContext();

    for (FrameGraphResourceHandle resourceHandle : node->InputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            if (texture->IsDepth())
                continue;
            SafePtr<Texture> debugTexture = Texture::CreateColorTexture2D(graphicsContext,
                texture->GetDimensions().width / 2, texture->GetDimensions().height / 2, texture->GetFormat(), TextureUsageType::eSampled, false, texture->GetName() + "ImGUI Debug");
            m_DebugTextures.emplace(texture->GetName(), debugTexture);
            m_IsDebugOpen.emplace(texture->GetName(), false);
        }
    }
    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = Texture::CreateColorTexture2D(graphicsContext,
                texture->GetDimensions().width / 2, texture->GetDimensions().height / 2, texture->GetFormat(), TextureUsageType::eSampled, false, texture->GetName() + "ImGUI Debug");
            m_DebugTextures.emplace(texture->GetName(), debugTexture);
            m_IsDebugOpen.emplace(texture->GetName(), false);
        }
    }
}

void GBufferPass::BeginFrame()
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    ClearDrawCommands();
}

void GBufferPass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    auto& renderer = ApplicationBase::GetRenderer();
    uint32_t frameIndex = renderer.GetCurrentFrameIndex();
    for (auto& [hash, drawCommand] : m_DrawCommands[frameIndex])
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        SubMeshTransformArray& transforms = worldRenderer->GetTransforms(frameIndex, hash);
        TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(frameIndex);

        // should render custom material
        renderer.Draw(cmdBuffer, drawCommand.Mesh, transformBuffer.Buffer, GetID(), transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}

void GBufferPass::PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    Renderer& renderer = ApplicationBase::GetRenderer();

    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            if (texture->IsDepth())
                continue;
            SafePtr<Texture> debugTexture = m_DebugTextures[texture->GetName()];
            if (m_IsDebugOpen[texture->GetName()] == false)
                continue;
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderer.Blit(cmdBuffer, texture, debugTexture);
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }

    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = m_DebugTextures[texture->GetName()];
            if (m_IsDebugOpen[texture->GetName()] == false)
                continue;
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
        m_IsDebugOpen[name] = ImGui::TreeNode(name.c_str()); 
        if (m_IsDebugOpen[name])
        {
            ImGui::Image((ImTextureID)(uint64_t)texture->GetBindlessTextureHandle(), ImVec2(windowWidth, windowWidth / textureRatio));
            ImGui::TreePop();
        }
    }
}

void GBufferPass::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex)
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
