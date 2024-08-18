#pragma once
#include "Enums.h"
#include <vma/vk_mem_alloc.h>

namespace lne
{
struct UniformBinding
{
    uint32_t SetIndex;
    uint32_t BindingIndex;
    uint32_t Size;
    vk::ShaderStageFlags Stages;
};

struct DescriptorSet
{
    uint32_t SetIndex;
    std::unordered_map<std::string, UniformBinding> UniformBuffers;
};

struct UniformElement
{
    std::string UniformBuffer;
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
}