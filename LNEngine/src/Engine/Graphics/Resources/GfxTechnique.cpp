#include "lnepch.h"
#include "GfxTechnique.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Resources/Shader.h"
#include "Graphics/Resources/StorageBuffer.h"
#include "Graphics/FrameGraph/FrameGraph.h"

namespace lne
{
GfxTechnique::GfxTechnique(const Desc& desc)
    : m_Name(desc.Name), m_State(desc.TechniqueState)
{
    for (const PassBindingDesc& passDesc : desc.Passes)
    {
        PassBinding binding{
            .PassId = MakePassID(passDesc.PassName),
            .PassEffect = passDesc.PassEffect,
        };
        m_Passes.push_back(binding);
    }
}

PipelineHandle GfxTechnique::CreateOrGetPipeline(PassID passID, SafePtr<FrameGraph> frameGraph)
{
    auto it = std::find_if(m_Passes.begin(), m_Passes.end(), [passID](const PassBinding& p)
                           {
                               return p.PassId == passID;
                           }
    );
    if (it == m_Passes.end())
        return PipelineHandle{};
    std::scoped_lock lock(m_PipelineMutex);
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

SafePtr<GfxPipeline> GfxTechnique::GetPipeline(PassID passID)
{
    auto it = std::find_if(m_Passes.begin(), m_Passes.end(), [passID](const PassBinding& p)
                           {
                               return p.PassId == passID;
                           }
    );
    if (it == m_Passes.end())
        return nullptr;
    std::scoped_lock lock(m_PipelineMutex);
    return it->PassEffect->GetPipeline(it->PipelineHandle);
}

lne::SafePtr<lne::Effect> GfxTechnique::GetPassEffect(PassID passID)
{
    // Not thread-safe, should be called during initialization
    auto it = std::find_if(m_Passes.begin(), m_Passes.end(), [passID](const PassBinding& p)
                           {
                               return p.PassId == passID;
                           }
    );
    if (it == m_Passes.end())
        return nullptr;
    return it->PassEffect;
}

std::vector<MaterialPassSlot> GfxTechnique::AllocateMaterialSlots()
{
    std::vector<MaterialPassSlot> slots;
    slots.reserve(m_Passes.size());
    for (const auto& pass : m_Passes)
    {
        MaterialSlot slot = pass.PassEffect->AllocateMaterialSlot();
        auto it = pass.PassEffect->GetShader()->GetReflectedData().PushConstants.find("matPC");
        vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        if (it == pass.PassEffect->GetShader()->GetReflectedData().PushConstants.end())
            LNE_ERROR("Shader used in technique {} pass '{}' does not have 'matPC' push constant", m_Name, pass.PassId);

        stageFlags = it->second.Stages;
        slots.emplace_back(pass.PassId, slot, stageFlags);
    }
    return slots;
}
}
