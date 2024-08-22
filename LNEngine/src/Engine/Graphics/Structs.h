#pragma once
#include "Enums.h"
#include "../vendor/VMA/vk_mem_alloc.h"

namespace lne
{
struct BufferBinding
{
    uint32_t SetIndex;
    uint32_t BindingIndex;
    uint32_t Size;
    vk::ShaderStageFlags Stages;
};

struct DescriptorSet
{
    uint32_t SetIndex;
    std::unordered_map<std::string, BufferBinding> UniformBuffers;
    std::unordered_map<std::string, BufferBinding> StorageBuffers;
};

struct UniformElement
{
    uint32_t SetIndex;
    uint32_t BindingIndex;
    uint32_t Offset;
    uint32_t Size;
    UniformElementType::Enum Type;
};

struct BufferAllocation
{
    vk::Buffer Buffer;
    VmaAllocation Allocation;
    VmaAllocationInfo AllocationInfo;
    vk::MemoryPropertyFlags MemoryFlags;
};

struct ImageAllocation
{
    vk::Image Image;
    VmaAllocation Allocation;
    VmaAllocationInfo AllocationInfo;
};
}
