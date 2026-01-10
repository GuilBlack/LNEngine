#pragma once
#include "Enums.h"
#include <variant>
#include "../vendor/VMA/vk_mem_alloc.h"

namespace lne
{

using ResourceDeletionHandle = void*;
using BindlessImageHandle = uint32_t;
using MaterialSlot = uint32_t;
using PassID = uint64_t;

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

struct SubMesh
{
    std::string Name;
    uint32_t BaseVertex;
    uint32_t BaseIndex;
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t MaterialIndex;
    AABB BoundingBox;

    glm::mat4 WorldTransform = glm::mat4(1.0f);
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

struct MaterialPassSlot
{
    MaterialSlot                            Slot;
    vk::ShaderStageFlags                    Stages;
};

struct PipelineHandle
{
    uint64_t                                H1 = 0; // used for indexing
    uint64_t                                H2 = 0; // verification tag
    bool operator==(const PipelineHandle& o) const { return H1 == o.H1 && H2 == o.H2; }
    bool operator!=(const PipelineHandle& o) const { return !(*this == o); }
};

// find a better name for this.
struct MaterialPipelineHash
{
    PassID                                  PassId;
    uint64_t                                FrameGraphHash;

    bool operator==(const MaterialPipelineHash& other) const
    {
        return PassId == other.PassId && FrameGraphHash == other.FrameGraphHash;
    }
    bool operator!=(const MaterialPipelineHash& o) const
    {
        return !(*this == o);
    }
};

struct MeshletPushConstants
{
    MaterialSlot                            MatId;
    uint32_t                                InstanceOffset;
};

struct LightGPUData
{
    LightType::Enum                        Type;
    glm::vec3                              Position;
    glm::vec3                              Direction;
    glm::vec3                              Color;
    float                                  Intensity;
    float                                  Range;
    float                                  SpotAngle;
};
}
