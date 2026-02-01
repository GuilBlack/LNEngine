#include "SsrPass.h"
#include "Core/ApplicationBase.h"
#include "Core/Utils/Profiling.h"

#include "Graphics/Renderer.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/FrameGraph/FrameGraph.h"
#include "Graphics/Resources/GfxTechnique.h"
#include "Graphics/Resources/Effect.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/Mesh.h"

namespace lne
{
void SsrPass::OnBind(FrameGraph* frameGraph, FrameGraphNode* node)
{
    auto& renderer = ApplicationBase::Get().GetRenderer();
    auto effect = renderer.CreateOrGetEffect(ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\PostProcess\\Ssr.glsl");

    GfxTechniqueDesc techDesc{};
    techDesc.Name = "SsrTechnique";
    techDesc.TechniqueState.Cull = lne::ECullMode::None;
    techDesc.TechniqueState.Fill = lne::EFillMode::Solid;
    techDesc.TechniqueState.Transparency = lne::TransparencyMode::eOpaque;
    techDesc.TechniqueState.DepthMode = lne::DepthMode::eNone;

    PassBindingDesc passDesc{};
    passDesc.PassName = "SsrPass";
    passDesc.PassEffect = effect;
    techDesc.Passes.push_back(passDesc);
    SafePtr technique = renderer.CreateOrGetTechnique(techDesc);

    m_Material = lnnew Material(technique);

    for (FrameGraphResourceHandle resourceHandle : node->InputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Name == "GBufferNormal")
        {
            SafePtr<Texture> normalTexture = resource.Resource.GetAs<Texture>();
            m_Material->SetTexture("tNormal", normalTexture);
            continue;
        }
        if (resource.Name == "Depth")
        {
            SafePtr<Texture> depthTexture = resource.Resource.GetAs<Texture>();
            m_Material->SetTexture("tDepth", depthTexture);
            continue;
        }
        if (resource.Name == "Lighting")
        {
            SafePtr<Texture> sceneTexture = resource.Resource.GetAs<Texture>();
            m_Material->SetTexture("tScene", sceneTexture);
            continue;
        }
    }
}

void SsrPass::Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL);

    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    auto lightBuffer = worldRenderer->GetLightBufferGPU(renderer.GetCurrentFrameIndex());
    renderer.DrawFullscreenQuad(cmdBuffer, m_Material, lightBuffer, GetID());
}

void SsrPass::OnResize(FrameGraph* frameGraph, FrameGraphNode* node)
{
    for (FrameGraphResourceHandle resourceHandle : node->InputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Name == "GBufferNormal")
        {
            SafePtr<Texture> normalTexture = resource.Resource.GetAs<Texture>();
            m_Material->SetTexture("tNormal", normalTexture);
        }
        if (resource.Name == "Depth")
        {
            SafePtr<Texture> depthTexture = resource.Resource.GetAs<Texture>();
            m_Material->SetTexture("tDepth", depthTexture);
        }
        if (resource.Name == "Lighting")
        {
            SafePtr<Texture> sceneTexture = resource.Resource.GetAs<Texture>();
            m_Material->SetTexture("tScene", sceneTexture);
        }
    }
}

}

