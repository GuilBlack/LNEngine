#include "Core/Utils/Log.h"
#include "Graphics/DynamicDescriptorAllocator.h"
#include "Graphics/Structs.h"
#include "Graphics/GfxContext.h"
#include "Graphics/VulkanUtils.h"

namespace lne
{
DynamicDescriptorAllocator::DynamicDescriptorAllocator(GfxContext* ctx, 
    const std::vector<vk::DescriptorPoolSize>& setBindingSize,
    std::string_view debugName,
    u32 numSetsPerPool, float growthFactor, 
    vk::DescriptorPoolCreateFlags poolFlags)
    : m_Context(ctx), m_DebugName(debugName), m_GrowthFactor(growthFactor), m_NextPoolSizes(setBindingSize),
    m_PoolFlags(poolFlags), m_MaxSetsPerPool(numSetsPerPool)
{
    m_SetToPoolIndexMap.max_load_factor(0.5f);
    m_SetToPoolIndexMap.reserve(m_MaxSetsPerPool); // Reserve space for the initial pool size
    AllocateNewPool();
}

DynamicDescriptorAllocator::~DynamicDescriptorAllocator()
{
    Clear();
    auto device = m_Context->GetDevice();
    for (auto& pool : m_PoolInfos)
        device.destroyDescriptorPool(pool.Pool);
}

DynamicDescriptorAllocator::DynamicDescriptorAllocator(DynamicDescriptorAllocator&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.m_Mutex);

    m_Context = std::move(other.m_Context);
    m_DebugName = std::move(other.m_DebugName);
    m_GrowthFactor = std::move(other.m_GrowthFactor);
    m_NextPoolSizes = std::move(other.m_NextPoolSizes);
    m_MaxSetsPerPool = std::move(other.m_MaxSetsPerPool);

    m_PoolInfos = std::move(other.m_PoolInfos);
    m_FreePoolIndices = std::move(other.m_FreePoolIndices);
    m_SetToPoolIndexMap = std::move(other.m_SetToPoolIndexMap);
}

DynamicDescriptorAllocator& DynamicDescriptorAllocator::operator=(DynamicDescriptorAllocator&& other) noexcept
{
    if (this == &other)
        return *this;
    std::lock_guard<std::mutex> lock(other.m_Mutex);

    m_Context = std::move(other.m_Context);
    m_DebugName = std::move(other.m_DebugName);
    m_GrowthFactor = std::move(other.m_GrowthFactor);
    m_NextPoolSizes = std::move(other.m_NextPoolSizes);
    m_MaxSetsPerPool = std::move(other.m_MaxSetsPerPool);

    m_PoolInfos = std::move(other.m_PoolInfos);
    m_FreePoolIndices = std::move(other.m_FreePoolIndices);
    m_SetToPoolIndexMap = std::move(other.m_SetToPoolIndexMap);

    return *this;
}

void DynamicDescriptorAllocator::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    vk::Device device = m_Context->GetDevice();
    for (auto& poolInfo : m_PoolInfos)
    {
        device.resetDescriptorPool(poolInfo.Pool);
        poolInfo.UsedSets = 0;
    }

    if ((VkFlags)(m_PoolFlags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet))
        m_SetToPoolIndexMap.clear();

    m_FreePoolIndices.clear();
    for (u32 i = 0; i < m_PoolInfos.size(); ++i)
        m_FreePoolIndices.emplace_back(i);
}

vk::DescriptorSet DynamicDescriptorAllocator::Allocate(vk::DescriptorSetLayout layout)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_FreePoolIndices.empty())
        AllocateNewPool();

    u32 poolIdx = m_FreePoolIndices.back();
    auto& poolInfo = m_PoolInfos[poolIdx];

    vk::DescriptorSetAllocateInfo allocInfo{
        poolInfo.Pool,
        layout
    };
    try
    {
        auto result = m_Context->GetDevice().allocateDescriptorSets(allocInfo);
        if ((VkFlags)(m_PoolFlags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet))
            m_SetToPoolIndexMap[result.back()] = poolIdx;

        ++poolInfo.UsedSets;
        if (poolInfo.UsedSets >= poolInfo.CapacitySets)
            m_FreePoolIndices.pop_back();

        return result.back();
    }
    catch (std::exception& e)
    {
        (void)e;
        LNE_WARN("DynamicDescriptorAllocator: {} \n Exception: {}", m_DebugName, e.what());
        AllocateNewPool();
        poolIdx = m_FreePoolIndices.back();
        allocInfo.descriptorPool = m_PoolInfos[poolIdx].Pool;
        auto result = m_Context->GetDevice().allocateDescriptorSets(allocInfo);
        ++m_PoolInfos[poolIdx].UsedSets;
        return result.back();
    }
}

vk::DescriptorSet DynamicDescriptorAllocator::Allocate(vk::DescriptorSetLayout layout, u32 variableCount)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_FreePoolIndices.empty())
        AllocateNewPool();

    vk::DescriptorSetVariableDescriptorCountAllocateInfo varInfo;
    varInfo.descriptorSetCount = 1;
    varInfo.pDescriptorCounts = &variableCount;

    u32 poolIdx = m_FreePoolIndices.back();
    auto& poolInfo = m_PoolInfos[poolIdx];
    
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = poolInfo.Pool;
    allocInfo.setSetLayouts(layout);
    allocInfo.pNext = (variableCount > 0) ? &varInfo : nullptr;
    try
    {
        auto result = m_Context->GetDevice().allocateDescriptorSets(allocInfo);
        if ((VkFlags)(m_PoolFlags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet))
            m_SetToPoolIndexMap[result.back()] = poolIdx;

        ++poolInfo.UsedSets;
        if (poolInfo.UsedSets >= poolInfo.CapacitySets)
            m_FreePoolIndices.pop_back();

        return result.back();
    }
    catch (std::exception& e)
    {
        (void)e;
        LNE_WARN("DynamicDescriptorAllocator: {} \n Exception: {}", m_DebugName, e.what());
        AllocateNewPool();
        poolIdx = m_FreePoolIndices.back();
        allocInfo.descriptorPool = m_PoolInfos[poolIdx].Pool;
        auto result = m_Context->GetDevice().allocateDescriptorSets(allocInfo);
        ++m_PoolInfos[poolIdx].UsedSets;
        return result.back();
    }
}

void DynamicDescriptorAllocator::Free(vk::DescriptorSet set)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if ((VkFlags)(m_PoolFlags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet) == 0)
    {
        LNE_WARN(std::format("{}: trying to free a set but pool was not created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT", m_DebugName));
        return;
    }

    auto it = m_SetToPoolIndexMap.find(set);
    if (it == m_SetToPoolIndexMap.end())
    {
        LNE_WARN(std::format("{}: trying to free a set not owned by this allocator", m_DebugName));
        return;
    }

    u32 poolIdx = it->second;
    auto device = m_Context->GetDevice();
    auto& poolInfo = m_PoolInfos[poolIdx];
    VK_CHECK(device.freeDescriptorSets(poolInfo.Pool, 1, &set));
    m_SetToPoolIndexMap.erase(it);

    bool wasFull = (poolInfo.UsedSets == poolInfo.CapacitySets);
    if (poolInfo.UsedSets > 0)
        --poolInfo.UsedSets;

    if (wasFull)
        m_FreePoolIndices.push_back(poolIdx);

}

void DynamicDescriptorAllocator::AllocateNewPool()
{
    vk::DescriptorPoolCreateInfo descPoolCI{
        m_PoolFlags,
        m_MaxSetsPerPool,
    };

    std::vector<vk::DescriptorPoolSize> sizes;
    sizes.reserve(m_NextPoolSizes.size());
    for (auto size : m_NextPoolSizes)
    {
        size.descriptorCount *= m_MaxSetsPerPool;
        sizes.push_back(size);
    }
    descPoolCI.setPoolSizes(sizes);

    m_PoolInfos.emplace_back(PoolInfo{ 
        .Pool = m_Context->GetDevice().createDescriptorPool(descPoolCI),
        .CapacitySets = m_MaxSetsPerPool,
    });
    m_FreePoolIndices.emplace_back((u32)m_PoolInfos.size() - 1);

    m_Context->SetVkObjectName(m_PoolInfos.back().Pool, std::format("{}_{}", m_DebugName, m_FreePoolIndices.back()));

    m_MaxSetsPerPool = std::min((u32)(m_MaxSetsPerPool * m_GrowthFactor), UINT32_MAX);
}
}
