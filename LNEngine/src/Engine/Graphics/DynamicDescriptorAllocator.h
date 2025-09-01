#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/GlobalUtils.h"

namespace lne
{
class DynamicDescriptorAllocator : public RefCountBase
{
public:
    DynamicDescriptorAllocator(class GfxContext* ctx,
        const std::vector<vk::DescriptorPoolSize>& setBindingsSize,
        std::string_view debugName = "",
        uint32_t numSetsPerPool = 16, float growthFactor = 1.f, 
        vk::DescriptorPoolCreateFlags poolFlags = {});
    virtual ~DynamicDescriptorAllocator();

    DynamicDescriptorAllocator(DynamicDescriptorAllocator&& other) noexcept;
    DynamicDescriptorAllocator& operator=(DynamicDescriptorAllocator&& other) noexcept;

    vk::DescriptorSet   Allocate(vk::DescriptorSetLayout layout);
    vk::DescriptorSet   Allocate(vk::DescriptorSetLayout layout, uint32_t variableCount);
    void                Free(vk::DescriptorSet set);
    void                Clear();

private:
    struct SetHasher
    {
        std::size_t operator()(const vk::DescriptorSet& set) const
        {
            // use this hash function since VkDescriptorSet can be low-enthropy
            return GlobalUtils::HashU64((uint64_t)(VkDescriptorSet)set);
        }
    };
    using SetMap = std::unordered_map<vk::DescriptorSet, uint32_t, SetHasher>;

    struct PoolInfo
    {
        vk::DescriptorPool  Pool;
        uint32_t            UsedSets = 0;
        uint32_t            CapacitySets = 0;
    };
private:
    class GfxContext*                       m_Context;
    std::string                             m_DebugName{};
    float                                   m_GrowthFactor{};
    std::vector<vk::DescriptorPoolSize>     m_NextPoolSizes{};
    vk::DescriptorPoolCreateFlags           m_PoolFlags{};
    uint32_t                                m_MaxSetsPerPool{ 16 };

    std::vector<PoolInfo>                   m_PoolInfos;
    std::vector<uint32_t>                   m_FreePoolIndices{};
    SetMap                                  m_SetToPoolIndexMap{};
    
    std::mutex                              m_Mutex{};

private:
    void AllocateNewPool();
};
}

