#include "lnepch.h"
#include "StorageBuffer.h"

#include <Core/Utils/Log.h>

#include "GfxContext.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "CommandBufferManager.h"
#include "DynamicDescriptorAllocator.h"
#include "Texture.h"
#include "Renderer.h"

namespace lne
{
StorageBuffer::StorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data, StorageBufferType::Enum type)
    : m_Context(ctx), m_Size(size)
{
    switch (type)
    {
    case StorageBufferType::eStatic:
        InitStatic(data);
        break;
    case StorageBufferType::eDynamic:
        InitDynamic();
        break;
    }
}

StorageBuffer::~StorageBuffer()
{
    BufferResourceDeletion bufferDeletion{
        .MainAllocation = m_Allocation,
        .StagingAllocation = m_StagingAllocation,
        .HasStaging = m_HasStagingBuffer,
    };
    ResourceDeletion deletion{
        .Type = ResourceType::eBuffer,
        .Resource = bufferDeletion,
    };
    m_Context->EnqueueResourceDeletion(deletion);
}

void StorageBuffer::CopyData(vk::CommandBuffer cb, const void* data, uint64_t size, uint64_t offset)
{
    if (m_Type == StorageBufferType::eStatic)
    {
        LNE_ERROR("Cannot copy data to a static buffer");
        return;
    }
    
    if (m_Allocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        vmaCopyMemoryToAllocation(m_Context->GetMemoryAllocator(), data, m_Allocation.Allocation, offset, size);

        // Ensure visibility if memory is not host-coherent
        if (!(m_Allocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            vmaFlushAllocation(m_Context->GetMemoryAllocator(), m_Allocation.Allocation, offset, size);
        }

        vk::BufferMemoryBarrier barrier{
            vk::AccessFlagBits::eHostWrite,
            vk::AccessFlagBits::eShaderRead,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            m_Allocation.Buffer,
            offset,
            size
        };
        
        vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eHost;
        vk::PipelineStageFlags dstStage =
            vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eVertexShader;

        cb.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags{}, nullptr, barrier, nullptr);

    }
    else
    {
        memcpy((uint8_t*)m_StagingAllocation.AllocationInfo.pMappedData + offset, data, size);
        VK_CHECK_C(vmaFlushAllocation(m_Context->GetMemoryAllocator(), m_StagingAllocation.Allocation, offset, size));

        vk::BufferMemoryBarrier barrier{
            vk::AccessFlagBits::eHostWrite,
            vk::AccessFlagBits::eTransferRead,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            m_StagingAllocation.Buffer,
            offset,
            size
        };

        vk::BufferCopy copyRegion{
            offset,
            offset,
            size
        };

        cb.copyBuffer(m_StagingAllocation.Buffer, m_Allocation.Buffer, copyRegion);

        vk::BufferMemoryBarrier barrier2{
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            m_Allocation.Buffer,
            offset,
            size
        };
    }
}

void StorageBuffer::InitStatic(const void* data)
{
    if (data == nullptr)
    {
        LNE_ERROR("Static buffer data is null");
        return;
    }
    vk::BufferCreateInfo bufferCI{
        {},
        m_Size,
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
    m_StagingAllocation = m_Context->AllocateStagingBuffer(m_Size);

    memcpy(m_StagingAllocation.AllocationInfo.pMappedData, data, m_Size);

    auto cmdBuffer = ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer();

    vk::BufferCopy copyRegion = vk::BufferCopy{
        0,
        0,
        m_Size
    };

    cmdBuffer.copyBuffer(m_StagingAllocation.Buffer, m_Allocation.Buffer, copyRegion);
}

void StorageBuffer::InitDynamic()
{
    vk::BufferCreateInfo bufferCI{
        {},
        m_Size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo allocCI{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT 
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT 
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };

    m_Context->AllocateBuffer(m_Allocation, bufferCI, allocCI);

    if (bool(m_Allocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible) == false)
    {
        bufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
        allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        m_Context->AllocateBuffer(m_StagingAllocation, bufferCI, allocCI);
    }
    else
    {
        m_HasStagingBuffer = false;
    }
}
}
