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
            .PassEffect = passDesc.PassEffect,
        };
        m_Passes.emplace(MakePassID(passDesc.PassName), binding);
    }
}

PipelineHandle GfxTechnique::CreateOrGetPipeline(PassID passID, SafePtr<FrameGraph> frameGraph)
{
    auto it = m_Passes.find(passID);
    if (it == m_Passes.end())
        return PipelineHandle{};

    std::scoped_lock lock(m_PipelineMutex);
    PassBinding& passBinding = it->second;
    if (passBinding.PipelineHandle != PipelineHandle{})
        return passBinding.PipelineHandle;
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
    desc.Shader = passBinding.PassEffect->GetShader();
    passBinding.PipelineHandle = passBinding.PassEffect->CreateOrGetPipeline(desc);
    return passBinding.PipelineHandle;
}

SafePtr<GfxPipeline> GfxTechnique::GetPipeline(PassID passID)
{
    auto it = m_Passes.find(passID);
    if (it == m_Passes.end())
        return nullptr;

    std::scoped_lock lock(m_PipelineMutex);
    return it->second.PassEffect->GetPipeline(it->second.PipelineHandle);
}

lne::SafePtr<lne::Effect> GfxTechnique::GetPassEffect(PassID passID)
{
    // Not thread-safe, should be called during initialization
    auto it = m_Passes.find(passID);
    if (it == m_Passes.end())
        return nullptr;
    return it->second.PassEffect;
}

FlatHashMap<PassID, MaterialPassSlot> GfxTechnique::AllocateMaterialSlots()
{
    FlatHashMap<PassID, MaterialPassSlot> slots;
    slots.reserve(m_Passes.size());
    for (const auto&[passId, passBinding] : m_Passes)
    {
        MaterialSlot slot = passBinding.PassEffect->AllocateMaterialSlot();
        auto it = passBinding.PassEffect->GetShader()->GetReflectedData().PushConstants.find("matPC");
        vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        if (it == passBinding.PassEffect->GetShader()->GetReflectedData().PushConstants.end())
            LNE_ERROR("Shader used in technique {} pass '{}' does not have 'matPC' push constant", m_Name, passId);

        stageFlags = it->second.Stages;
        slots.emplace(passId, MaterialPassSlot{ slot, stageFlags });
    }
    return slots;
}
}
