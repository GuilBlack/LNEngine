#include "lnepch.h"
#include "StorageBuffer.h"
#include "GfxContext.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "CommandBufferManager.h"
#include "DynamicDescriptorAllocator.h"
#include "Texture.h"

namespace lne
{
StorageBuffer::StorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data)
    : m_Context(ctx), m_Size(size)
{
    vk::BufferCreateInfo bufferCI{
        {},
        size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo allocCI{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .priority = 1.0f,
    };

    m_Context->AllocateBuffer(m_Allocation, bufferCI, allocCI);

    BufferAllocation stagingAllocation = m_Context->AllocateStagingBuffer(size);

    memcpy(stagingAllocation.AllocationInfo.pMappedData, data, size);

    auto cmdBuffer = m_Context->GetTransferCommandBufferManager().BeginSingleTimeCommands();

    vk::BufferCopy copyRegion = vk::BufferCopy{
        0,
        0,
        size
    };

    cmdBuffer.copyBuffer(stagingAllocation.Buffer, m_Allocation.Buffer, copyRegion);

    m_Context->GetTransferCommandBufferManager().EndSingleTimeCommands();
    m_Context->FreeBuffer(stagingAllocation);
}

StorageBuffer::~StorageBuffer()
{
    m_Context->FreeBuffer(m_Allocation);
}
}
