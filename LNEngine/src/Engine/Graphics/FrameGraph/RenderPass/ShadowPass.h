#pragma once
#include "Engine/Graphics/FrameGraph/RenderPass/RenderPass.h"
#include "Engine/Graphics/FrameGraph/RenderPass/Interfaces/SceneComponentsAdder.h"

namespace lne
{
class ShadowPass : RenderPass, ICustomDrawStaticMeshesAdder, ILightAdder
{

public:
    void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;

    void BeginFrame() override;

    void PreExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) override;
    void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;
    void PostExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) override;

    void EndFrame() override;

    void OnResize(FrameGraph* frameGraph, FrameGraphNode* node) override;

    void OnImGuiRender() override;

    void AddStaticMeshes(ComponentView<TransformComponent, StaticMeshComponent>& entitiesView) override;
    void AddLights(ComponentView<TransformComponent, LightComponent>& lightView) override;

private:
};
}
