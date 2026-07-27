#pragma once
#include "Engine/Graphics/FrameGraph/RenderPass/RenderPass.h"

namespace lne
{
class Material;

class SsrPass : public RenderPass
{
public:
    SsrPass()
    {
        m_Name = "SsrPass";
    }

    void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) override;

    void Execute(CommandBuffer* cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) override;

    void OnResize(FrameGraph* frameGraph, FrameGraphNode* node) override;

private:
    SafePtr<Material>       m_Material;
};
}
