#pragma once
#include "Enums.h"
#include <variant>
#include "../vendor/VMA/vk_mem_alloc.h"
#include "Engine/Core/Utils/Defines.h"

namespace lne
{

using ResourceDeletionHandle = void*;
using BindlessImageHandle = u32;
using MaterialSlot = u32;
using PassID = u64;

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
    u32                                ElapsedFrames;
};

struct SubMesh
{
    std::string Name;
    u32    BaseVertex;
    u32    BaseIndex;
    u32    VertexCount;
    u32    IndexCount;
    u32    MaterialIndex;
    AABB        BoundingBox;

    u32    BaseMeshlet;
    u32    MeshletCount;

    glm::mat4   WorldTransform = glm::mat4(1.0f);
};

struct StaticMeshHash
{
    // TODO: Should probably change this to an ID instead of a pointer...
    u64                                     MeshAddress;
    u32                                     SubMeshIndex;

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
    u64                                     H1 = 0; // used for indexing
    u64                                     H2 = 0; // verification tag
    bool operator==(const PipelineHandle& o) const { return H1 == o.H1 && H2 == o.H2; }
    bool operator!=(const PipelineHandle& o) const { return !(*this == o); }
};

// find a better name for this.
struct MaterialPipelineHash
{
    PassID                                  PassId;
    u64                                     FrameGraphHash;

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
    u32                                     InstanceOffset;
    u32                                     BaseMeshlet;
    u32                                     MeshletCount;
};

struct LightGPUData
{
    LightType::Enum                        Type;
    glm::vec3                              Position;
    glm::vec3                              Direction;
    glm::vec3                              Color;
    float                                  Intensity;
    float                                  Range;
    float                                  Falloff;
    float                                  SpotAngle;
};
}
