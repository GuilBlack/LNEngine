#include "RenderPass.h"
#include "Graphics/Resources/Mesh.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/GfxTechnique.h"
#include "../FrameGraph.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Renderer.h"

namespace lne
{

void RenderPass::OnBindInternal(FrameGraph* frameGraph, FrameGraphNode* node)
{
    m_ID = MakePassID(m_Name);
    OnBind(frameGraph, node);
}

lne::PassID MakePassID(std::string_view name)
{
    // FNV-1a hash
    constexpr uint64_t prime = 0x100000001b3;
    uint64_t hash = 0xcbf29ce484222325;
    for (char c : name)
    {
        hash ^= c;
        hash *= prime;
    }
    return hash;
}

IDrawStaticMeshes::IDrawStaticMeshes()
{
    uint32_t maxFrames = ApplicationBase::GetRenderer().GetGfxContext()->GetMaxFramesInFlight();
    m_DrawCommands.resize(maxFrames);
}

void IDrawStaticMeshes::ClearDrawCommands()
{
    uint32_t frameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    m_DrawCommands[frameIndex].clear();
}

}
