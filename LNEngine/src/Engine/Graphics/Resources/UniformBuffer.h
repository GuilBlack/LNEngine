#pragma once
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/SafePtr.h"
#include "../vendor/VMA/vk_mem_alloc.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/GfxContext.h"

namespace lne
{
class UniformBuffer : public RefCountBase
{
public:
    UniformBuffer() = default;
    UniformBuffer(SafePtr<class GfxContext> ctx, uint32_t size);
    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;

    ~UniformBuffer();
    void Nuke();
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
    bool m_IsNuked{ false };
};
}
