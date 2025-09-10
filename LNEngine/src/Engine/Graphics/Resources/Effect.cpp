#include "Effect.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Resources/Shader.h"
#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/StorageBuffer.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Renderer.h"

namespace lne
{

Effect::Effect(SafePtr<GfxContext> context, const std::string& shaderPath)
    : m_Context(context), m_Pipelines(8)
{
    m_Shader = ApplicationBase::GetRenderer().CreateOrGetShader(shaderPath);
    if (!m_Shader)
    {
        LNE_ERROR("Failed to create effect from shader '{}'", shaderPath);
        return;
    }
    m_Name = m_Shader->GetName();
    uint32_t matSetIndex = m_Shader->GetSetIndex(ShaderSetIndexType::eMaterial);
    uint32_t maxFramesInFlight = m_Context->GetMaxFramesInFlight();

    const DescriptorSet& matSet = m_Shader->GetReflectedData().DescriptorSets.at(matSetIndex);

    if (matSet.StorageBuffers.empty())
    {
        LNE_ERROR("Effect '{}' has no storage buffer in the material set", m_Name);
        return;
    }
    vk::DescriptorSetLayout matDescSet = m_Shader->GetDescriptorSetLayouts().at(matSetIndex);
    size_t numStorageBuffer = matSet.StorageBuffers.size();
    m_Bank.Items.resize(numStorageBuffer);
    std::vector<std::vector<vk::WriteDescriptorSet>> writeDescSets;
    writeDescSets.resize(maxFramesInFlight);
    std::vector<std::vector<vk::DescriptorBufferInfo>> bufferInfos;
    bufferInfos.resize(maxFramesInFlight);
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
    {
        writeDescSets[i].reserve(numStorageBuffer);
        bufferInfos[i].reserve(numStorageBuffer);
    }
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
    {
        m_Bank.FrameDescSets.emplace_back(m_Context->AllocateDescriptorSet(
            m_Context->GetStorageOnlyDescriptorSetLayout((uint32_t)numStorageBuffer),
            DescriptorType::eStorageOnly));
    }

    uint32_t initialCapacity{};
    switch (m_Shader->GetShaderDomain())
    {
    case ShaderDomain::eMesh:
        initialCapacity = 256;
        break;
    case ShaderDomain::ePostProcess:
        initialCapacity = 1;
        break;
    default:
        initialCapacity = 16;
        break;
    }

    // creating the buffers & descriptor set writes
    for (const auto& [name, binding] : matSet.StorageBuffers)
    {
        SafePtr<StorageBuffer> buffer;
        uint32_t elementSize = binding.Size;
        BankItem& item = m_Bank.Items[binding.BindingIndex];
        item.ElementSize = elementSize;

        item.FrameBuffer.resize(maxFramesInFlight);
        for (uint32_t i = 0; i < maxFramesInFlight; i++)
        {
            item.FrameBuffer[i] = lnnew StorageBuffer(m_Context, 
                                                      (uint64_t)elementSize * initialCapacity, 
                                                      nullptr, StorageBufferType::Enum::eDynamic);
            bufferInfos[i].emplace_back(item.FrameBuffer[0]->GetDescriptorInfo());
            vk::WriteDescriptorSet writeDescSet{
                m_Bank.FrameDescSets[i],
                binding.BindingIndex,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &bufferInfos[i].back(),
                nullptr
            };
            writeDescSets[i].emplace_back(writeDescSet);
        }
    }
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
        m_Context->GetDevice().updateDescriptorSets(writeDescSets[i], nullptr);

    // creating the free slots
    m_Bank.Count = initialCapacity;
    m_Bank.FreeSlots.reserve(initialCapacity);

    for (int64_t i = initialCapacity - 1; i >= 0; --i)
        m_Bank.FreeSlots.push_back((uint32_t)i);
}

Effect::~Effect()
{
    m_Pipelines.clear();
    m_Bank.Items.clear();
    
    for (auto& descSet : m_Bank.FrameDescSets)
    {
        DescriptorSetDeletion descSetDel{
            .Type = DescriptorType::eStorageOnly,
            .DescriptorSet = descSet
        };
        m_Context->EnqueueResourceDeletion(
            ResourceDeletion{
                .Type = ResourceType::Enum::eDescriptorSet,
                .Resource = descSetDel
            }
        );
    }
}

PipelineHandle Effect::CreateOrGetPipeline(GraphicsPipelineDescV2& pipelineDesc)
{
    pipelineDesc.Shader = m_Shader;
    PipelineHandle hash = MakeHandle(pipelineDesc);
    std::lock_guard<std::mutex> lock(m_PipelinesMutex);
    if (m_Pipelines.contains(hash))
        return hash;
    SafePtr newPipeline = lnnew GfxPipeline(m_Shader, const_cast<GraphicsPipelineDescV2&>(pipelineDesc));
    m_Pipelines.emplace(hash, newPipeline);
    return hash;
}

lne::SafePtr<lne::GfxPipeline> Effect::GetPipeline(PipelineHandle hash)
{
    std::lock_guard<std::mutex> lock(m_PipelinesMutex);
    auto it = m_Pipelines.find(hash);
    if (it == m_Pipelines.end())
        return nullptr;
    return it->second;
}

lne::MaterialSlot Effect::AllocateMaterialSlot()
{
    std::lock_guard<std::mutex> lock(m_SlotAllocMutex);

    MaterialSlot slot;
    if (m_Bank.FreeSlots.empty())
    {
        GrowFreeSlots();
        if (m_DirtyFrames == 0)
            ApplicationBase::GetRenderer().AddDirtyEffect(SafePtr(this));
        m_DirtyFrames = m_Context->GetMaxFramesInFlight();
    }
    slot = m_Bank.FreeSlots.back();
    m_Bank.FreeSlots.pop_back();
    return slot;
}

void Effect::FreeMaterialSlot(MaterialSlot slot)
{
    std::lock_guard<std::mutex> lock(m_SlotAllocMutex);
    m_Bank.FreeSlots.push_back(slot);
}

void Effect::GrowFreeSlots()
{
    uint32_t oldCount = m_Bank.Count;
    uint32_t newCount = oldCount * 2;
    m_Bank.Count = newCount;
    m_Bank.FreeSlots.reserve(newCount);

    for (uint64_t i = newCount - 1; i >= oldCount; --i)
        m_Bank.FreeSlots.push_back((uint32_t)i);
}

void Effect::GrowBank(vk::CommandBuffer cmdBuffer, uint32_t currentFrameInFlight)
{
    std::vector<vk::WriteDescriptorSet> writeDescSets;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    for (auto& bankItem : m_Bank.Items)
    {
        bankItem.FrameBuffer[currentFrameInFlight]->Grow(cmdBuffer, m_Bank.Count * bankItem.ElementSize);
        bufferInfos.emplace_back(bankItem.FrameBuffer[currentFrameInFlight]->GetDescriptorInfo());
        vk::WriteDescriptorSet writeDescSet{
            m_Bank.FrameDescSets[currentFrameInFlight],
            static_cast<uint32_t>(&bankItem - &m_Bank.Items[0]),
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &bufferInfos.back(),
            nullptr
        };
        writeDescSets.emplace_back(writeDescSet);
    }
    m_Context->GetDevice().updateDescriptorSets(writeDescSets, nullptr);
}

void Effect::CopyMaterialDataToBuffer(vk::CommandBuffer cmdBuffer,
                                      uint32_t currentFrameInFlight,
                                      MaterialSlot matSlot,
                                      uint32_t binding,
                                      void* data)
{
    auto& item = m_Bank.Items[binding];
    item.FrameBuffer[currentFrameInFlight]->CopyData(cmdBuffer, data, item.ElementSize, matSlot * item.ElementSize);
}

lne::PipelineHandle Effect::MakeHandle(const lne::GraphicsPipelineDescV2& d)
{
    // arbitrary large primes
    size_t s1 = 0x41788a801d56c693ull; // A
    size_t s2 = 0x72ec9023ea2a1533ull; // B

    auto mix = [&](size_t& s)
        {
            lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.CullMode));
            lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.Fill));
            lne::GlobalUtils::HashCombine(s, static_cast<size_t>(d.TransparencyMode));
            lne::GlobalUtils::HashCombine(s, static_cast<size_t>(d.DeriveDepthFromTransparency));
            lne::GlobalUtils::HashCombine(s, reinterpret_cast<size_t>(d.FrameGraph.GetPtr()));
            if (!d.DeriveDepthFromTransparency)
            {
                lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.DepthMode));
                lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.DepthCompareOp));
            }
        };

    mix(s1);
    mix(s2);

    PipelineHandle out;
    out.H1 = static_cast<uint64_t>(s1);
    out.H2 = static_cast<uint64_t>(s2);
    return out;
}

}
