#include "lnepch.h"
#include "GfxTechnique.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Resources/Shader.h"
#include "Graphics/Resources/Mesh.h"
#include "Graphics/FrameGraph/FrameGraph.h"

lne::GfxTechnique::GfxTechnique(const Desc& desc)
    : m_Name(desc.Name), m_State(desc.TechniqueState)
{
    for (const PassBindingDesc& passDesc : desc.Passes)
    {
        PassBinding binding{
            .PassName = MakePassID(passDesc.PassName),
            .PassEffect = passDesc.PassEffect,
        };
        m_Passes.push_back(binding);
    }
}

lne::PipelineHandle lne::GfxTechnique::CreateOrGetPipeline(PassID passID, SafePtr<FrameGraph> frameGraph)
{
    auto it = std::find_if(m_Passes.begin(), m_Passes.end(), [passID](const PassBinding& p) 
                           { return p.PassName == passID; }
    );
    if (it == m_Passes.end())
        return PipelineHandle{};
    if (it->PipelineHandle != PipelineHandle{})
        return it->PipelineHandle;
    GraphicsPipelineDescV2 desc{};
    desc.CullMode = m_State.Cull;
    desc.Fill = m_State.Fill;
    desc.TransparencyMode = m_State.Transparency;
    desc.DeriveDepthFromTransparency = m_State.DeriveDepthFromTransparency;
    desc.FrameGraph = frameGraph.GetPtr();
    if (!m_State.DeriveDepthFromTransparency)
    {
        desc.DepthMode = m_State.DepthMode;
        desc.DepthCompareOp = m_State.DepthCompareOp;
    }
    desc.Shader = it->PassEffect->GetShader();
    it->PipelineHandle = it->PassEffect->CreateOrGetPipeline(desc);
    return it->PipelineHandle;
}

lne::SafePtr<lne::GfxPipeline> lne::GfxTechnique::GetPipeline(PassID passID, PipelineHandle handle)
{
    auto it = std::find_if(m_Passes.begin(), m_Passes.end(), [passID](const PassBinding& p)
                           { return p.PassName == passID; }
    );
    if (it == m_Passes.end())
        return nullptr;
    return it->PassEffect->GetPipeline(handle);
}
