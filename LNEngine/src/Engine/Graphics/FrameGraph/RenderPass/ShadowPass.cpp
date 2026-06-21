#include "ShadowPass.h"

#include "ECS/ecs.h"
#include "Scene/Components.h"

#include "Graphics/Renderer.h"
#include "Graphics/WorldRenderer.h"

#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/Mesh.h"

namespace lne
{

void ShadowPass::OnBind(FrameGraph* frameGraph, FrameGraphNode* node)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::BeginFrame()
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::PreExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::PostExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::EndFrame()
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::OnResize(FrameGraph* frameGraph, FrameGraphNode* node)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::OnImGuiRender()
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ShadowPass::AddStaticMeshes(ComponentView<TransformComponent, StaticMeshComponent>& entitiesView)
{
    for (auto& idx : entitiesView)
    {
        auto [transform, mesh] = entitiesView.Get(idx);
        // I must do something here...
    }
}

void ShadowPass::AddLights(ComponentView<TransformComponent, LightComponent>& lightView)
{
}

}
