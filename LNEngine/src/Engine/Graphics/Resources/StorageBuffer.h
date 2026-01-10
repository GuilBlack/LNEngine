#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/Enums.h"

namespace lne
{
class GfxLoader;
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
    vk::MemoryPropertyFlags GetMemoryFlags() const { return m_Allocation.MemoryFlags; }

    uint64_t GetSize() const { return m_Size; }
    void CopyData(vk::CommandBuffer cb, const void* data, uint64_t size, uint64_t offset = 0);
    void Grow(vk::CommandBuffer cb, uint64_t newSize, bool shouldCopyData = true);
protected:
    SafePtr<class GfxContext> m_Context;

private:
    friend class GfxLoader;
    BufferAllocation m_Allocation;
    BufferAllocation m_StagingAllocation;
    uint64_t m_Size;
    vk::MemoryPropertyFlags m_MemoryFlags;
    StorageBufferType::Enum m_Type;
    bool m_HasStagingBuffer{ true };

private:
    StorageBuffer(SafePtr<class GfxContext> ctx, BufferAllocation oldAllocation, BufferAllocation oldStagingAllocation, bool hasStagingBuffer, StorageBufferType::Enum type);
    void InitStatic(const void* data);
    void InitDynamic();
    void CopyBufferToBuffer(vk::CommandBuffer cb, 
                            BufferAllocation src, BufferAllocation dst, 
                            uint64_t size, 
                            uint64_t srcOffset = 0, uint64_t dstOffset = 0);
};

// Has its own descriptor set layout
class StandaloneStorageBuffer : public StorageBuffer
{
public:
    StandaloneStorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data, StorageBufferType::Enum type = StorageBufferType::eStatic);
    ~StandaloneStorageBuffer();

    vk::DescriptorSet GetDescSet() const { return m_DescSet; }

private:
    vk::DescriptorSet m_DescSet;
};
}
