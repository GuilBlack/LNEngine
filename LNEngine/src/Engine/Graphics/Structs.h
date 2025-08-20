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
    uint32_t                                SetIndex;
    uint32_t                                BindingIndex;
    uint32_t                                Offset;
    uint32_t                                Size;
    UniformElementType::Enum                Type;
};

struct BufferAllocation
{
    vk::Buffer                              Buffer;
    VmaAllocation                           Allocation;
    VmaAllocationInfo                       AllocationInfo;
    vk::MemoryPropertyFlags                 MemoryFlags;
};

struct ImageAllocation
{
    vk::Image                               Image;
    VmaAllocation                           Allocation;
    VmaAllocationInfo                       AllocationInfo;
};

struct AABB
{
    glm::vec3                               Min;
    glm::vec3                               Max;
};


struct TextureResourceDeletion
{
    vk::ImageView                           ImageView;
    ImageAllocation                         Allocation;
    TextureUsageType::Enum                  UsageType;
    BindlessImageHandle                     BindlessTextureHandle;
    BindlessImageHandle                     BindlessStorageHandle;
    bool                                    OwnsAllocation;
};

struct ImageViewDeletion
{
    vk::ImageView                           ImageView;
    TextureUsageType::Enum                  UsageType;
    BindlessImageHandle                     BindlessTextureHandle;
};

struct BufferResourceDeletion
{
    BufferAllocation                        MainAllocation;
    BufferAllocation                        StagingAllocation;
    bool                                    HasStaging;
};

struct PipelineResourceDeletion
{
    vk::Pipeline                            Pipeline;
    vk::PipelineLayout                      Layout;
};

struct ShaderResourceDeletion
{
    std::vector<vk::DescriptorSetLayout>    DescriptorSetLayouts;
    std::vector<vk::ShaderModule>           ShaderModules;
};

struct DescriptorSetDeletion
{
    DescriptorType::Enum                    Type;
    vk::DescriptorSet                       DescriptorSet;
};

struct ResourceDeletion
{
    ResourceType::Enum                      Type;
    std::variant<
        TextureResourceDeletion,
        BufferResourceDeletion,
        ImageViewDeletion,
        PipelineResourceDeletion,
        ShaderResourceDeletion,
        DescriptorSetDeletion>              Resource;
    uint32_t                                ElapsedFrames;
};

struct StaticMeshHash
{
    // TODO: Should probably change this to an ID instead of a pointer...
    uint64_t                                MeshAddress;
    uint32_t                                SubMeshIndex;
    
    bool operator==(const StaticMeshHash& other) const
    {
        return MeshAddress == other.MeshAddress && SubMeshIndex == other.SubMeshIndex;
    }
};
}

namespace std
{
// based on boost's hash_combine
template <>
struct hash<lne::StaticMeshHash>
{
    std::size_t operator()(const lne::StaticMeshHash& k) const noexcept
    {
        std::size_t seed = std::hash<uint64_t>()(k.MeshAddress);
        seed ^= std::hash<uint32_t>()(k.SubMeshIndex) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
}
