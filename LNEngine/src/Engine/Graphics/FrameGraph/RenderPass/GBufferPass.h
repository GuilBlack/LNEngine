#pragma once
#include <Engine/Graphics/FrameGraph/RenderPass/RenderPass.h>

namespace lne
{
class GBufferPass : public lne::RenderPass, public IDrawStaticMeshes
{
public:
    GBufferPass()
    {
        m_Name = "GBufferPass";
    }

    void OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void BeginFrame() override;

    void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    void OnImGuiRender() override;

    void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex) override;

private:
    std::unordered_map<std::string, SafePtr<class Texture>> m_DebugTextures{};
    std::unordered_map<std::string, bool> m_IsDebugOpen{};
};
}

