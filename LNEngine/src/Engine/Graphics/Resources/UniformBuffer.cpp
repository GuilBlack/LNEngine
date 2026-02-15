#include "lnepch.h"
#include "UniformBuffer.h"
#include "Engine/Graphics/GfxContext.h"

namespace lne
{
UniformBuffer::UniformBuffer(SafePtr<class GfxContext> ctx, u32 size)
    : m_Context(ctx), m_Size(size)
{
    vk::BufferCreateInfo bufferCI{
    {},
    size,
    vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
    vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo allocCI{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT 
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT 
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };

    m_Context->AllocateBuffer(m_MainAllocation, bufferCI, allocCI);

    if (bool(m_MainAllocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible) == false)
    {
        bufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
        allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        m_Context->AllocateBuffer(m_StagingAllocation, bufferCI, allocCI);
    }
}

UniformBuffer::UniformBuffer(UniformBuffer && other) noexcept
{
    m_Context = std::move(other.m_Context);
    m_MainAllocation = std::move(other.m_MainAllocation);
    m_StagingAllocation = std::move(other.m_StagingAllocation);
    m_Size = other.m_Size;
}

UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept
{
    m_Context = std::move(other.m_Context);
    m_MainAllocation = std::move(other.m_MainAllocation);
    m_StagingAllocation = std::move(other.m_StagingAllocation);
    m_Size = other.m_Size;
    return *this;
}

UniformBuffer::~UniformBuffer()
{
    Nuke();
}

void UniformBuffer::Nuke()
{
    if (m_IsNuked)
        return;
    BufferResourceDeletion bufferDeletion{
        .MainAllocation = m_MainAllocation,
        .StagingAllocation = m_StagingAllocation,
        .HasStaging = bool(m_MainAllocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible) == false,
    };
    ResourceDeletion deletion{
        .Type = ResourceType::eBuffer,
        .Resource = bufferDeletion,
    };
    m_Context->EnqueueResourceDeletion(deletion);
    m_IsNuked = true;
}

void UniformBuffer::CopyData(vk::CommandBuffer cb, const void* data, u32 size, u32 offset)
{
    if (m_MainAllocation.MemoryFlags & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        vmaCopyMemoryToAllocation(m_Context->GetMemoryAllocator(), data, m_MainAllocation.Allocation, offset, size);

        vk::BufferMemoryBarrier barrier{
            vk::AccessFlagBits::eHostWrite,
            vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            m_MainAllocation.Buffer,
            offset,
            size
        };

        //cb.pipelineBarrier(
        //    vk::PipelineStageFlagBits::eHost,
        //    vk::PipelineStageFlagBits::eVertexShader,
        //    {},
        //    {},
        //    barrier,
        //    {}
        //);
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

        //cb.pipelineBarrier(
        //    vk::PipelineStageFlagBits::eHost,
        //    vk::PipelineStageFlagBits::eTransfer,
        //    {},
        //    {},
        //    barrier,
        //    {}
        //);

        vk::BufferCopy copyRegion{
            offset,
            offset,
            size
        };

        cb.copyBuffer(m_StagingAllocation.Buffer, m_MainAllocation.Buffer, copyRegion);

        vk::BufferMemoryBarrier barrier2{
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            m_MainAllocation.Buffer,
            offset,
            size
        };

        //cb.pipelineBarrier(
        //    vk::PipelineStageFlagBits::eTransfer,
        //    vk::PipelineStageFlagBits::eVertexShader,
        //    {},
        //    {},
        //    barrier2,
        //    {}
        //);
    }
}
}
