#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Utils/_Defines.h"

namespace lne
{
class DynamicDescriptorAllocator : public RefCountBase
{
public:
    DynamicDescriptorAllocator(const SafePtr<class GfxContext>& ctx,
        std::vector<vk::DescriptorPoolSize> setBindingsSize,
        std::string_view debugName = "",
        uint32_t numSetsPerPool = 16, float growthFactor = 1.f, 
        vk::DescriptorPoolCreateFlags poolFlags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    virtual ~DynamicDescriptorAllocator();

    DynamicDescriptorAllocator(DynamicDescriptorAllocator&& other) noexcept;
    DynamicDescriptorAllocator& operator=(DynamicDescriptorAllocator&& other) noexcept;

    vk::DescriptorSet Allocate(vk::DescriptorSetLayout layout);
    void Clear();

private:
    SafePtr<class GfxContext> m_Context;
    float m_GrowthFactor{};
    std::vector<vk::DescriptorPoolSize> m_NextPoolSizes{};
    std::vector<vk::DescriptorPool> m_Pools{};
    int32_t m_CurrentlyUsedPool{ -1 };
    std::string m_DebugName{};

private:
    void AllocateNewPool();
};
}

