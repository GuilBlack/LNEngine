#include "lnepch.h"
#include "FrameGraph.h"
#include "DynamicDescriptorAllocator.h"
#include "Texture.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "CommandBufferManager.h"
#include "RendererWorld.h"

namespace lne
{
RendererWorld::RendererWorld(const SafePtr<FrameGraph>& frameGraph)
    : m_FrameGraph(frameGraph)
{}

void RendererWorld::Render(EntityRegistry& registry)
{
    auto& renderer = ApplicationBase::GetRenderer();

    renderer.BeginFrame();
    renderer.PushLabel(renderer.GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer(), "Frame");

    // frame graph execution here I guess

    renderer.PopLabel(renderer.GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer());
    renderer.EndFrame();
}
}
