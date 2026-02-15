#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/Enums.h"
#include "Engine/Core/Utils/Defines.h"

namespace lne
{
class GfxLoader;
class StorageBuffer : public RefCountBase
{
public:
    StorageBuffer(SafePtr<class GfxContext> ctx,
                  u64 size, const void* data,
                  StorageBufferType::Enum type = StorageBufferType::eStatic);
    virtual ~StorageBuffer();

    vk::DescriptorBufferInfo            GetDescriptorInfo() const
    {
        return vk::DescriptorBufferInfo{
            m_Allocation.Buffer,
            0,
            m_Size
        };
    }
    vk::MemoryPropertyFlags             GetMemoryFlags() const { return m_Allocation.MemoryFlags; }

    u64                                 GetSize() const { return m_Size; }
    void                                CopyData(vk::CommandBuffer cb, const void* data,
                                                 u64 size, u64 offset = 0);
    virtual void                        Grow(vk::CommandBuffer cb, u64 newSize,
                                             bool shouldCopyData = true);
protected:
    SafePtr<class GfxContext>       m_Context;

private:
    friend class GfxLoader;
    BufferAllocation                m_Allocation;
    BufferAllocation                m_StagingAllocation;
    u64                             m_Size;
    vk::MemoryPropertyFlags         m_MemoryFlags;
    StorageBufferType::Enum         m_Type;
    bool                            m_HasStagingBuffer{ true };

private:
    StorageBuffer(SafePtr<class GfxContext> ctx, 
                  BufferAllocation oldAllocation, BufferAllocation oldStagingAllocation,
                  bool hasStagingBuffer, StorageBufferType::Enum type);

    void                                InitStatic(const void* data);
    void                                InitDynamic();
    void                                CopyBufferToBuffer(vk::CommandBuffer cb, 
                                                           BufferAllocation src, BufferAllocation dst,
                                                           u64 size,
                                                           u64 srcOffset = 0, u64 dstOffset = 0);
};

// Has its own descriptor set layout
class StandaloneStorageBuffer : public StorageBuffer
{
public:
    StandaloneStorageBuffer(SafePtr<class GfxContext> ctx, u64 size, const void* data, StorageBufferType::Enum type = StorageBufferType::eStatic);
    ~StandaloneStorageBuffer();
    void Grow(vk::CommandBuffer cb, u64 newSize, bool shouldCopyData = true) override;

    vk::DescriptorSet                   GetDescSet() const { return m_DescSet; }

private:
    vk::DescriptorSet               m_DescSet;
};
}
