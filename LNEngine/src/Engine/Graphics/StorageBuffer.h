#pragma once
#include "Engine/Core/SafePtr.h"
#include "Structs.h"
#include "Enums.h"

namespace lne
{
class StorageBuffer : public RefCountBase
{
public:
    StorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data, StorageBufferType::Enum type = StorageBufferType::eStatic);
    virtual ~StorageBuffer();

    vk::DescriptorBufferInfo GetDescriptorInfo() const
    {
        return vk::DescriptorBufferInfo{
            m_Allocation.Buffer,
            0,
            m_Size
        };
    }

    void CopyData(vk::CommandBuffer cb, const void* data, uint64_t size, uint64_t offset = 0);

private:
    SafePtr<class GfxContext> m_Context;
    BufferAllocation m_Allocation;
    BufferAllocation m_StagingAllocation;
    uint64_t m_Size;
    vk::MemoryPropertyFlags m_MemoryFlags;
    StorageBufferType::Enum m_Type;
    bool m_HasStagingBuffer{true};

private:
    void InitStatic(const void* data);
    void InitDynamic();
};
}
