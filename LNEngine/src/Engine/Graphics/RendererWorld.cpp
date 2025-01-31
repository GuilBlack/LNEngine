#include "lnepch.h"
#include "FrameGraph.h"
#include "Texture.h"
#include "RendererWorld.h"

namespace lne
{
RendererWorld::RendererWorld(const SafePtr<FrameGraph>& frameGraph)
    : m_FrameGraph(frameGraph)
{}

void RendererWorld::Render(EntityRegistry& registry)
{
    
}
}
