#include "LightingPass.h"

#include <Core/ApplicationBase.h>
#include <Core/Window.h>
#include <Graphics/Renderer.h>
#include <Graphics/FrameGraph/FrameGraph.h>
#include "Core/Utils/Profiling.h"
#include <Graphics/Resources/Pipeline.h>
#include <Graphics/Resources/Material.h>
#include <Graphics/Resources/Texture.h>
#include <Graphics/Resources/Mesh.h>
#include "../../Resources/Effect.h"
#include "../../Resources/GfxTechnique.h"

namespace lne
{
void LightingPass::OnBind(FrameGraph* frameGraph, FrameGraphNode* node)
{
    Renderer& renderer = ApplicationBase::GetRenderer();

    SafePtr skyboxEffect = renderer.CreateOrGetEffect(ApplicationBase::GetAssetsPath()
                                                      + "Engine\\Shaders\\Lighting.glsl");

	GfxTechniqueDesc techDesc{};
	techDesc.Name = "LightingTechnique";
	techDesc.TechniqueState.Cull = ECullMode::None;
	techDesc.TechniqueState.Fill = EFillMode::Solid;
	techDesc.TechniqueState.Transparency = TransparencyMode::eOpaque;
	techDesc.TechniqueState.DepthMode = DepthMode::eNone;

	PassBindingDesc passDesc{};
	passDesc.PassName = "LightingPass";
	passDesc.PassEffect = skyboxEffect;
	techDesc.Passes.push_back(passDesc);
    SafePtr technique = renderer.CreateOrGetTechnique(techDesc);

	m_Material = lnnew Material(technique);
    
    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = Texture::CreateColorTexture2D(ApplicationBase::GetWindow().GetGfxContext(),
                texture->GetDimensions().width / 2, texture->GetDimensions().height / 2, texture->GetFormat(), TextureUsageType::eSampled, false, texture->GetName() + "ImGUI Debug");
            m_DebugTexture = debugTexture;
        }
	}
	for (FrameGraphResourceHandle handle : node->InputResources)
	{
		FrameGraphResource* resource = frameGraph->GetResource(handle);
		if (resource->Name == "GBufferPosition")
		{
			SafePtr<Texture> positionTexture = resource->Resource.GetAs<Texture>();
            m_Material->SetTexture("tPosition", positionTexture);
		}
		if (resource->Name == "GBufferNormal")
		{
			SafePtr<Texture> normalTexture = resource->Resource.GetAs<Texture>();
            m_Material->SetTexture("tNormal", normalTexture);
        }
        if (resource->Name == "GBufferColor")
        {
            SafePtr<Texture> colorTexture = resource->Resource.GetAs<Texture>();
            m_Material->SetTexture("tAlbedo", colorTexture);
        }
        if (resource->Name == "GBufferMetalRough")
        {
            SafePtr<Texture> colorTexture = resource->Resource.GetAs<Texture>();
            m_Material->SetTexture("tMetalnessRoughness", colorTexture);
		}
	}
}

void LightingPass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    Renderer& renderer = ApplicationBase::GetRenderer();
    renderer.DrawFullscreenQuad(cmdBuffer, m_Material, GetID());
}

void LightingPass::PostExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    if (m_IsDebugOpen == false)
        return;
    
    Renderer& renderer = ApplicationBase::GetRenderer();
    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);
        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = m_DebugTexture;
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderer.Blit(cmdBuffer, texture, debugTexture);
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}

void LightingPass::OnImGuiRender()
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    float textureRatio = (float)m_DebugTexture->GetDimensions().width / (float)m_DebugTexture->GetDimensions().height;
    float windowWidth = ImGui::GetWindowWidth();
    m_IsDebugOpen = ImGui::TreeNode("Lighting Pass Output");
    if (m_IsDebugOpen)
    {
        ImGui::Image((ImTextureID)(uint64_t)m_DebugTexture->GetBindlessTextureHandle(), ImVec2(windowWidth, windowWidth / textureRatio));
        ImGui::TreePop();
    }
}
}
