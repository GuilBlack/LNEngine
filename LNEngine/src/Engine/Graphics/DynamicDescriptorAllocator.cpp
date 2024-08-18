#include "DynamicDescriptorAllocator.h"
#include "Structs.h"
#include "GfxContext.h"
#include "Core/Utils/Log.h"

namespace lne
{
DynamicDescriptorAllocator::DynamicDescriptorAllocator(const SafePtr<GfxContext>& ctx, 
    std::vector<vk::DescriptorPoolSize> setBindingSize,
    std::string_view debugName,
    uint32_t numSetsPerPool, float growthFactor, 
    vk::DescriptorPoolCreateFlags poolFlags)
    : m_Context(ctx), m_GrowthFactor(growthFactor), m_DebugName(debugName)
{
    for (auto& descPoolSize : setBindingSize)
    {
        descPoolSize.descriptorCount *= numSetsPerPool;
        m_NextPoolSizes.emplace_back(descPoolSize);
    }

    // TODO: Add support for other descriptor types
    AllocateNewPool();
}

DynamicDescriptorAllocator::~DynamicDescriptorAllocator()
{
    Clear();
    auto device = m_Context->GetDevice();
    for (auto& pool : m_Pools)
        device.destroyDescriptorPool(pool);
}

DynamicDescriptorAllocator::DynamicDescriptorAllocator(DynamicDescriptorAllocator&& other) noexcept
{
    m_Context = std::move(other.m_Context);
    m_GrowthFactor = std::move(other.m_GrowthFactor);
    m_NextPoolSizes = std::move(other.m_NextPoolSizes);
    m_Pools = std::move(other.m_Pools);
    m_CurrentlyUsedPool = std::move(other.m_CurrentlyUsedPool);
    m_DebugName = std::move(other.m_DebugName);
}

DynamicDescriptorAllocator& DynamicDescriptorAllocator::operator=(DynamicDescriptorAllocator&& other) noexcept
{
    m_Context = std::move(other.m_Context);
    m_GrowthFactor = std::move(other.m_GrowthFactor);
    m_NextPoolSizes = std::move(other.m_NextPoolSizes);
    m_Pools = std::move(other.m_Pools);
    m_CurrentlyUsedPool = std::move(other.m_CurrentlyUsedPool);
    m_DebugName = std::move(other.m_DebugName);
    return *this;
}

void DynamicDescriptorAllocator::Clear()
{
    vk::Device device = m_Context->GetDevice();
    for (auto& pool : m_Pools)
    {
        device.resetDescriptorPool(pool);
    }
    m_CurrentlyUsedPool = 0;
}

vk::DescriptorSet DynamicDescriptorAllocator::Allocate(vk::DescriptorSetLayout layout)
{
    vk::DescriptorSetAllocateInfo allocInfo{
        m_Pools[m_CurrentlyUsedPool],
        layout
    };
    try
    {
        auto result = m_Context->GetDevice().allocateDescriptorSets(allocInfo);
        return result.back();
    }
    catch (std::exception& e)
    {
        LNE_WARN("DynamicDescriptorAllocator: {} \n Exception: {}", m_DebugName, e.what());
        AllocateNewPool();
        allocInfo.descriptorPool = m_Pools[m_CurrentlyUsedPool];
        auto result = m_Context->GetDevice().allocateDescriptorSets(allocInfo);
        return result.back();
    }
}

void DynamicDescriptorAllocator::AllocateNewPool()
{
    uint32_t maxDescSet = 0;
    for (const auto& poolSize : m_NextPoolSizes)
        maxDescSet += poolSize.descriptorCount;

    vk::DescriptorPoolCreateInfo descPoolCI{
        {},
        maxDescSet,
        m_NextPoolSizes
    };

    m_Pools.emplace_back(m_Context->GetDevice().createDescriptorPool(descPoolCI));
    ++m_CurrentlyUsedPool;

    m_Context->SetVkObjectName(m_Pools.back(), m_DebugName);

    for (auto& poolSize : m_NextPoolSizes)
        poolSize.descriptorCount = (uint32_t)(m_GrowthFactor * poolSize.descriptorCount);
}
}
