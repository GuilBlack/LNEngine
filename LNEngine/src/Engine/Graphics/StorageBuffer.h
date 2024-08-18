#pragma once
#include "Engine/Core/SafePtr.h"
#include "Structs.h"

namespace lne
{
class StorageBuffer : public RefCountBase
{
public:
    StorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data);
    virtual ~StorageBuffer();

    vk::DescriptorBufferInfo GetDescriptorInfo() const
    {
        return vk::DescriptorBufferInfo{
            m_Allocation.Buffer,
            0,
            m_Size
        };
    }

private:
    SafePtr<class GfxContext> m_Context;
    BufferAllocation m_Allocation;
    uint64_t m_Size;
    vk::MemoryPropertyFlags m_MemoryFlags;
};
}
