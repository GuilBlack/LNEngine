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
    m_Pipelines.Clear();
}

PipelineHandle Effect::CreateOrGetPipeline(GraphicsPipelineDescV2& pipelineDesc)
{
    pipelineDesc.Shader = m_Shader;
    PipelineHandle hash = MakeHandle(pipelineDesc);
    std::lock_guard<std::mutex> lock(m_PipelinesMutex);
    if (m_Pipelines.Has(hash))
        return hash;
    SafePtr newPipeline = lnnew GfxPipeline(m_Shader, const_cast<GraphicsPipelineDescV2&>(pipelineDesc));
    m_Pipelines.Add(hash, newPipeline);
    return hash;
}

lne::SafePtr<lne::GfxPipeline> Effect::GetPipeline(PipelineHandle hash)
{
    std::lock_guard<std::mutex> lock(m_PipelinesMutex);
    return m_Pipelines.Get(hash);
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

PipelineCache::PipelineCache(size_t initialCapacity /*= 16*/)
{
    if (initialCapacity == 0) 
        initialCapacity = 1;
    m_Capacity = GlobalUtils::NextPow2(initialCapacity);
    m_Pipelines.resize(m_Capacity);
    m_UsedIndices.reserve(m_Capacity);
}

bool PipelineCache::Add(const PipelineHandle& handle, SafePtr<GfxPipeline> pipeline)
{
    for (;;)
    {
        const uint32_t idx = IndexFor(handle.H1, m_Capacity);
        PipelineCacheItem& slot = m_Pipelines[idx];

        if (!slot.Pipeline)
        {
            // Empty: place
            slot.Handle = handle;
            slot.Pipeline = pipeline;
            m_UsedIndices.push_back(idx);
            ++m_Size;
            return true; // inserted
        }

        // Occupied: same entry?
        if (slot.Handle == handle)
        {
            slot.Pipeline = pipeline; // update
            return false;             // updated
        }

        // True collision: grow and retry
        GrowAndRehash(m_Capacity * 2);
    }
}

bool PipelineCache::Has(const PipelineHandle& handle) const
{
    if (m_Capacity == 0) return false;
    const uint32_t idx = IndexFor(handle.H1, m_Capacity);
    const PipelineCacheItem& slot = m_Pipelines[idx];
    return slot.Pipeline && slot.Handle == handle;
}

lne::SafePtr<lne::GfxPipeline> PipelineCache::Get(const PipelineHandle& handle) const
{
    if (m_Capacity == 0) 
        return {};
    const uint32_t idx = IndexFor(handle.H1, m_Capacity);
    const PipelineCacheItem& slot = m_Pipelines[idx];
    if (slot.Pipeline && slot.Handle == handle)
        return slot.Pipeline;
    return {};
}

void PipelineCache::GrowAndRehash(size_t newCapacity)
{
    newCapacity = GlobalUtils::NextPow2(newCapacity);
    LNE_ASSERT(newCapacity != m_Capacity, "How did we create this many pipelines?");

    for (;;)
    {
        std::vector<PipelineCacheItem> newTable(newCapacity);

        bool collision = false;
        for (uint32_t idx : m_UsedIndices)
        {
            const PipelineCacheItem& it = m_Pipelines[idx];
            if (!it.Pipeline)
                continue;

            const uint32_t nidx = IndexFor(it.Handle.H1, newCapacity);
            PipelineCacheItem& tgt = newTable[nidx];

            if (tgt.Pipeline)
            {
                if (!(tgt.Handle == it.Handle))
                {
                    collision = true;
                    break;
                }
            }

            tgt = it;
        }

        if (!collision)
        {
            m_Pipelines.swap(newTable);
            m_Capacity = newCapacity;

            // rebuild used indices
            m_UsedIndices.clear();
            m_UsedIndices.reserve(m_Size);
            for (uint32_t i = 0; i < m_Capacity; ++i)
                if (m_Pipelines[i].Pipeline) m_UsedIndices.push_back(i);
            return;
        }

        newCapacity *= 2; // try larger
    }
}

}
