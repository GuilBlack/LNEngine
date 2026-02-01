#pragma once
#include "Engine/Graphics/FrameGraph/RenderPass/RenderPass.h"

namespace lne
{
class Texture;
class WorldRenderer;
class FrameGraph;
struct FrameGraphNode;

class ScenePyramidPass : public lne::RenderPass
{
public:
    ScenePyramidPass(std::string_view passName, std::string_view inputTextureName);

    virtual void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

private:
    std::string m_InputTextureName;
    lne::SafePtr<lne::Texture> m_OutputTexture{};
};

}
