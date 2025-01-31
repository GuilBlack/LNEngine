#include "lnepch.h"
#include "StorageBuffer.h"
#include "GfxContext.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "CommandBufferManager.h"
#include "DynamicDescriptorAllocator.h"
#include "Texture.h"
#include "Renderer.h"

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

    // TODO: make it work with the async loader
    m_StagingAllocation = m_Context->AllocateStagingBuffer(size);

    memcpy(m_StagingAllocation.AllocationInfo.pMappedData, data, size);

    auto cmdBuffer = ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer();

    vk::BufferCopy copyRegion = vk::BufferCopy{
        0,
        0,
        size
    };

    cmdBuffer.copyBuffer(m_StagingAllocation.Buffer, m_Allocation.Buffer, copyRegion);
    
}

StorageBuffer::~StorageBuffer()
{
    BufferResourceDeletion bufferDeletion{
        .MainAllocation = m_Allocation,
        .StagingAllocation = m_StagingAllocation,
        .HasStaging = true,
    };
    ResourceDeletion deletion{
        .Type = ResourceType::eBuffer,
        .Resource = bufferDeletion,
    };
    m_Context->EnqueueResourceDeletion(deletion);
}
}
