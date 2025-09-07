#include "lnepch.h"
#include "StorageBuffer.h"

#include <Core/Utils/Log.h>

#include "Engine/Graphics/GfxContext.h"
#include "Core/ApplicationBase.h"
#include "Engine/Resources/GfxLoader.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Graphics/CommandPoolManager.h"
#include "Engine/Graphics/DynamicDescriptorAllocator.h"
#include "Engine/Graphics/Resources/Texture.h"

namespace lne
{
StorageBuffer::StorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data, StorageBufferType::Enum type)
    : m_Context(ctx), m_Size(size), m_Type(type)
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
                vk::PipelineStageFlagBits::eAllGraphics;

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
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{}, nullptr, barrier, nullptr);

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
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, 
                          vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eVertexShader, 
                           vk::DependencyFlags{}, nullptr, barrier2, nullptr);
    }
}

void StorageBuffer::Grow(vk::CommandBuffer cb, uint64_t newSize)
{
    if (newSize == m_Size)
        return;
    if (newSize <= m_Size)
    {
        LNE_ERROR("StorageBuffer::Grow - size must be greater than current size");
        return;
    }

    // 1. Create new buffer
    uint64_t oldSize = m_Size;
    m_Size = newSize;
    vk::BufferCreateInfo bufferCI{};
    VmaAllocationCreateInfo allocCI{};
    switch (m_Type)
    {
    case StorageBufferType::eStatic:
    {
        bufferCI = {
            {},
            m_Size,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
            vk::SharingMode::eExclusive,
        };
        allocCI = {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.0f,
        };
        break;
    }
    case StorageBufferType::eDynamic:
    {
        bufferCI = {
            {},
            m_Size,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
            vk::SharingMode::eExclusive,
        };
        allocCI = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
                | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        };
        break;
    }
    }

    BufferAllocation newAllocation;
    m_Context->AllocateBuffer(newAllocation, bufferCI, allocCI);

    bool hasStagingBuffer = m_HasStagingBuffer;
    BufferAllocation newStagingAllocation;
    // 1.5. check if we need a staging buffer
    if (m_Type == StorageBufferType::eDynamic && bool(newAllocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible) == false)
    {
        bufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
        allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        m_Context->AllocateBuffer(newStagingAllocation, bufferCI, allocCI);
        hasStagingBuffer = true;
    }
    else
    {
        m_HasStagingBuffer = false;
    }

    // 2. Copy old data to new buffer
    CopyBufferToBuffer(cb, m_Allocation, newAllocation, oldSize);

    // 3. Free old buffer
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

    // 4. Update members
    m_Allocation = newAllocation;
    m_StagingAllocation = newStagingAllocation;
    m_HasStagingBuffer = hasStagingBuffer;
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
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo allocCI{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .priority = 1.0f,
    };

    m_Context->AllocateBuffer(m_Allocation, bufferCI, allocCI);

    ApplicationBase::GetRenderer().GetGfxLoader()->InitStaticStorageBuffer(SafePtr(this), data);
}

void StorageBuffer::InitDynamic()
{
    vk::BufferCreateInfo bufferCI{
        {},
        m_Size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
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

void StorageBuffer::CopyBufferToBuffer(vk::CommandBuffer cb, 
                                       BufferAllocation src, BufferAllocation dst, 
                                       uint64_t size, 
                                       uint64_t srcOffset /*= 0*/, uint64_t dstOffset /*= 0*/)
{
    if (src.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        if (!(src.MemoryFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
            vmaFlushAllocation(m_Context->GetMemoryAllocator(), src.Allocation, srcOffset, size);

        vk::BufferMemoryBarrier preCopySrcBarrier{
            vk::AccessFlagBits::eHostWrite,         // srcAccessMask
            vk::AccessFlagBits::eTransferRead,      // dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            src.Buffer,
            srcOffset,
            size
        };

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                           vk::PipelineStageFlagBits::eTransfer,
                           {}, {}, preCopySrcBarrier, {});
    }

    const vk::BufferCopy copy{ srcOffset, dstOffset, size };
    cb.copyBuffer(src.Buffer, dst.Buffer, copy);

    vk::AccessFlags dstAccess =
        vk::AccessFlagBits::eShaderRead;

    vk::PipelineStageFlags dstStages =
        vk::PipelineStageFlagBits::eAllGraphics |
        vk::PipelineStageFlagBits::eComputeShader;

    vk::BufferMemoryBarrier postCopyDstBarrier{
        vk::AccessFlagBits::eTransferWrite,
        dstAccess,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        dst.Buffer,
        dstOffset,
        size
    };

    cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                       dstStages,
                       {}, {}, postCopyDstBarrier, {});
}

StandaloneStorageBuffer::StandaloneStorageBuffer(SafePtr<class GfxContext> ctx, uint64_t size, const void* data, StorageBufferType::Enum type /*= StorageBufferType::eStatic*/)
    : StorageBuffer(ctx, size, data, type)
{
    m_DescSet = m_Context->AllocateDescriptorSet(ctx->GetStorageOnlyDescriptorSetLayout(1), DescriptorType::eStorageOnly);
    
    vk::DescriptorBufferInfo bufferInfo = GetDescriptorInfo();
    vk::WriteDescriptorSet writeDescSet{
        m_DescSet,
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &bufferInfo,
        nullptr
    };
    
    m_Context->GetDevice().updateDescriptorSets(writeDescSet, nullptr);
}

StandaloneStorageBuffer::~StandaloneStorageBuffer()
{
    DescriptorSetDeletion resourceDeletion{
               .Type = DescriptorType::eStorageOnly,
               .DescriptorSet = m_DescSet,
    };
    m_Context->EnqueueResourceDeletion(ResourceDeletion{
        .Type = ResourceType::eDescriptorSet,
        .Resource = resourceDeletion
    });
}

}
