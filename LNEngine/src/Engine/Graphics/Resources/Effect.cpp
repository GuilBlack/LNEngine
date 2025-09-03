#include "Effect.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Resources/Shader.h"
#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/StorageBuffer.h"

namespace lne
{

Effect::Effect(SafePtr<GfxContext> context, const std::string& shaderPath)
    : m_Context(context), m_Pipelines(8)
{
    m_Shader = m_Context->CreateShader(shaderPath);
    m_Name = m_Shader->GetName();
    uint32_t matSetIndex = m_Shader->GetSetIndex(ShaderSetIndexType::eMaterial);
    const DescriptorSet& matSet = m_Shader->GetReflectedData().DescriptorSets.at(matSetIndex);
    m_Bank.Items.resize(matSet.StorageBuffers.size());
    std::vector<vk::WriteDescriptorSet> writeDescSets;
    writeDescSets.reserve(matSet.StorageBuffers.size());

    for (const auto& [name, binding] : matSet.StorageBuffers)
    {
        SafePtr<StorageBuffer> buffer;
        uint32_t elementSize = binding.Size;
        uint32_t initialCapacity;
        switch (m_Shader->GetMaterialType())
        {
        case ShaderDomain::eMesh:
            initialCapacity = 256;
            break;
        case ShaderDomain::ePostProcess:
            initialCapacity = 4;
            break;
        default:
            initialCapacity = 16;
            break;
        }
        BankItem& item = m_Bank.Items[binding.BindingIndex];
        item.ElementSize = elementSize;
        item.Count = initialCapacity;
        item.FreeIndices.reserve(initialCapacity);

        for (uint32_t i = 0; i < initialCapacity; i++)
            item.FreeIndices.push_back(i);

        item.Buffer.resize(m_Context->GetMaxFramesInFlight());
        for (uint32_t i = 0; i < m_Context->GetMaxFramesInFlight(); i++)
        {
            item.Buffer[i] = lnnew StorageBuffer(m_Context, 
                                                 (uint64_t)elementSize * initialCapacity, 
                                                 nullptr, StorageBufferType::Enum::eDynamic);
        }
    }
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
