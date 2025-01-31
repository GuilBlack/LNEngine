#pragma once
#include <Core/SafePtr.h>

class FrameGraph;

namespace lne
{
class RendererWorld : public RefCountBase
{
public:
    RendererWorld(const SafePtr<FrameGraph>& frameGraph);

    void Render(class EntityRegistry& registry);

private:
    SafePtr<FrameGraph> m_FrameGraph{};
};
}
