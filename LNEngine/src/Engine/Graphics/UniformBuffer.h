#pragma once
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/SafePtr.h"
#include <vma/vk_mem_alloc.h>
#include "Structs.h"
#include "GfxContext.h"

namespace lne
{
class UniformBuffer
{
public:
    MOVABLE_ONLY(UniformBuffer);

    UniformBuffer(SafePtr<class GfxContext> ctx, uint32_t size);
    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;

    ~UniformBuffer() = default;
    void Destroy();
    vk::DescriptorBufferInfo GetDescriptorInfo() const 
    { 
        return vk::DescriptorBufferInfo{
            m_MainAllocation.Buffer,
            0,
            m_Size
        }; 
    }

    template<typename T> requires std::is_trivially_copyable_v<T> && (std::is_pointer_v<T> == false)
    void CopyData(vk::CommandBuffer cb, const T& data, uint32_t offset = 0)
    {
        CopyData(cb, (const void*)&data, sizeof(T), offset * sizeof(T));
    }
    void CopyData(vk::CommandBuffer cb, const void* data, uint32_t size, uint32_t byteOffset);

private:
    SafePtr<class GfxContext> m_Context;
    BufferAllocation m_MainAllocation;
    BufferAllocation m_StagingAllocation;
    uint32_t m_Size{ 0 };
};

class UniformBufferManager : public RefCountBase
{
public:
    MOVABLE_ONLY(UniformBufferManager);
    UniformBufferManager(SafePtr<class GfxContext> ctx, uint32_t size);
    UniformBufferManager(UniformBufferManager&& other) noexcept;

    ~UniformBufferManager();

    template<typename T> requires std::is_trivially_copyable_v<T>
    void CopyData(vk::CommandBuffer cb, const T& data, uint32_t byteOffset = 0)
    {
        m_Buffers[m_Context->GetCurrentFrameIndex()].CopyData(cb, data, byteOffset);
    }
    void CopyData(vk::CommandBuffer cb, const void* data, uint32_t size, uint32_t byteOffset)
    {
        m_Buffers[m_Context->GetCurrentFrameIndex()].CopyData(cb, data, size, byteOffset);
    }

    [[nodiscard]] UniformBuffer& GetCurrentBuffer() { return m_Buffers[m_Context->GetCurrentFrameIndex()]; };

    [[nodiscard]] auto begin() { return m_Buffers.begin(); }
    [[nodiscard]] auto end() { return m_Buffers.end(); }

    [[nodiscard]] auto cbegin() const { return m_Buffers.cbegin(); }
    [[nodiscard]] auto cend() const { return m_Buffers.cend(); }

private:
    SafePtr<class GfxContext> m_Context;
    std::vector<UniformBuffer> m_Buffers;
};
}
