#include "RenderPass.h"
#include "Interfaces/IDrawStaticMeshesAdder.h"
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
    constexpr u64 prime = 0x100000001b3;
    u64 hash = 0xcbf29ce484222325;
    for (char c : name)
    {
        hash ^= c;
        hash *= prime;
    }
    return hash;
}

IDrawStaticMeshesAdder::IDrawStaticMeshesAdder()
{
    u32 maxFrames = ApplicationBase::GetRenderer().GetGfxContext()->GetMaxFramesInFlight();
    m_DrawCommands.resize(maxFrames);
}

void IDrawStaticMeshesAdder::ClearDrawCommands()
{
    u32 frameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    m_DrawCommands[frameIndex].clear();
}
}
