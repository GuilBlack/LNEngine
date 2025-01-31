#pragma once
#include "Enums.h"
#include <variant>
#include "../vendor/VMA/vk_mem_alloc.h"

namespace lne
{

using ResourceDeletionHandle = void*;
using BindlessImageHandle = uint32_t;

struct BufferBinding
{
    uint32_t                SetIndex;
    uint32_t                BindingIndex;
    uint32_t                Size;
    vk::ShaderStageFlags    Stages;
};

struct DescriptorSet
{
    uint32_t                                        SetIndex;
    std::unordered_map<std::string, BufferBinding>  UniformBuffers;
    std::unordered_map<std::string, BufferBinding>  StorageBuffers;
};

struct UniformElement
{
    uint32_t                    SetIndex;
    uint32_t                    BindingIndex;
    uint32_t                    Offset;
    uint32_t                    Size;
    UniformElementType::Enum    Type;
};

struct BufferAllocation
{
    vk::Buffer              Buffer;
    VmaAllocation           Allocation;
    VmaAllocationInfo       AllocationInfo;
    vk::MemoryPropertyFlags MemoryFlags;
};

struct ImageAllocation
{
    vk::Image           Image;
    VmaAllocation       Allocation;
    VmaAllocationInfo   AllocationInfo;
};

struct AABB
{
    glm::vec3 Min;
    glm::vec3 Max;
};


struct TextureResourceDeletion
{
    vk::ImageView       ImageView;
    ImageAllocation     Allocation;
    BindlessImageHandle BindlessHandle;
    bool                OwnsAllocation;
};

struct BufferResourceDeletion
{
    BufferAllocation MainAllocation;
    BufferAllocation StagingAllocation;
    bool             HasStaging;
};

struct PipelineResourceDeletion
{
    vk::Pipeline        Pipeline;
    vk::PipelineLayout  Layout;
};

struct ShaderResourceDeletion
{
    std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
    std::vector<vk::ShaderModule> ShaderModules;
};

struct ResourceDeletion
{
    ResourceType::Enum      Type;
    std::variant<TextureResourceDeletion, BufferResourceDeletion, 
        PipelineResourceDeletion, ShaderResourceDeletion> Resource;
    uint32_t                ElapsedFrames;
};
}
