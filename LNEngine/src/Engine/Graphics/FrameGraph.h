#pragma once
#include "Engine/DataStructures/ObjectCache.h"
#include "Engine/Core/SafePtr.h"
#include "Enums.h"
#include "Framebuffer.h"

namespace lne
{
using FrameGraphResourceHandle = ObjectPoolHandle;
using FrameGraphNodeHandle = ObjectPoolHandle;

struct FrameGraphResourceBufferInfo
{
    size_t                  Size;
    vk::BufferUsageFlags    Flags;
};

struct FrameGraphResourceImageInfo
{
    vk::Extent3D            Extent;

    vk::Format              Format;
    vk::ImageUsageFlags     Flags;
    vk::ImageAspectFlags    Aspect;

    vk::AttachmentLoadOp    LoadOp;
};

struct FrameGraphResourceInfo
{
    union
    {
        FrameGraphResourceBufferInfo Buffer;
        FrameGraphResourceImageInfo Image;
    };
    bool External = false;
};

struct FrameGraphResource
{
    FrameGraphResourceType::Enum Type{};
    uint32_t RefCount{ 0 };
    FrameGraphResourceInfo Info{};
    SafePtr<RefCountBase> Resource{}; // Texture, Buffer, etc.

    FrameGraphNodeHandle Producer{ INVALID_OBJECT_POOL_HANDLE };
    FrameGraphResourceHandle ProducerResourceHandle{ INVALID_OBJECT_POOL_HANDLE };

    std::string Name{};
};

struct FrameGraphNode
{
    std::vector<FrameGraphResourceHandle> InputResources{};
    std::vector<FrameGraphResourceHandle> OutputResources{};

    Framebuffer Framebuffer{};

    std::vector<FrameGraphNodeHandle> Dependents{};

    std::string Name{};
    bool Enabled{ true };
};

struct FrameGraphResourceDesc
{
    FrameGraphResourceType::Enum Type;
    FrameGraphResourceInfo Info{};
    std::string Name{};
};

struct FrameGraphNodeDesc
{
    std::vector<FrameGraphResourceDesc> InputResources{};
    std::vector<FrameGraphResourceDesc> OutputResources{};

    std::string Name{};
    bool Enabled{ true };
};

class FrameGraphResourceDescBuilder
{
public:
    FrameGraphResourceDescBuilder() = default;
    ~FrameGraphResourceDescBuilder() = default;

    FrameGraphResourceDescBuilder& SetType(FrameGraphResourceType::Enum type)
    {
        m_Desc.Type = type;
        return *this;
    }
    
    FrameGraphResourceDescBuilder& SetBufferInfo(size_t size, vk::BufferUsageFlags flags)
    {
        m_BufferInfo.Size = size;
        m_BufferInfo.Flags = flags;
        return *this;
    }

    FrameGraphResourceDescBuilder& SetImageDimension(uint32_t width, uint32_t height, uint32_t depth = 1);
    FrameGraphResourceDescBuilder& SetImageFormat(vk::Format format)
    {
        m_ImageInfo.Format = format;
        return *this;
    }
    FrameGraphResourceDescBuilder& SetImageUsage(vk::ImageUsageFlags flags)
    {
        m_ImageInfo.Flags = flags;
        return *this;
    }
    FrameGraphResourceDescBuilder& SetImageAspect(vk::ImageAspectFlags aspect)
    {
        m_ImageInfo.Aspect = aspect;
        return *this;
    }
    FrameGraphResourceDescBuilder& SetImageLoadOp(vk::AttachmentLoadOp loadOp)
    {
        m_ImageInfo.LoadOp = loadOp;
        return *this;
    }
    
    FrameGraphResourceDescBuilder& SetExternal(bool external)
    {
        m_Desc.Info.External = external;
        return *this;
    }
    FrameGraphResourceDescBuilder& SetName(const std::string& name)
    {
        m_Desc.Name = name;
        return *this;
    }

    FrameGraphResourceDescBuilder& SetDefaultColorAttachmentInfos();
    FrameGraphResourceDescBuilder& SetDefaultDepthAttachmentInfos();

    FrameGraphResourceDesc Build();

private:
    FrameGraphResourceBufferInfo m_BufferInfo{};
    FrameGraphResourceImageInfo m_ImageInfo{};
    vk::Extent3D m_Extent{};
    FrameGraphResourceDesc m_Desc{};
};

class FrameGraphNodeDescBuilder
{
public:
    FrameGraphNodeDescBuilder() = default;
    ~FrameGraphNodeDescBuilder() = default;

    FrameGraphNodeDescBuilder& SetName(const std::string& name)
    {
        m_Desc.Name = name;
        return *this;
    }

    FrameGraphNodeDescBuilder& SetEnabled(bool enabled)
    {
        m_Desc.Enabled = enabled;
        return *this;
    }

    FrameGraphNodeDescBuilder& AddInputResource(const FrameGraphResourceDesc& desc)
    {
        m_Desc.InputResources.push_back(desc);
        return *this;
    }
    FrameGraphNodeDescBuilder& AddOutputResource(const FrameGraphResourceDesc& desc)
    {
        m_Desc.OutputResources.push_back(desc);
        return *this;
    }

    void Clear() { m_Desc = {}; }

    FrameGraphNodeDesc Build();

private:
    FrameGraphNodeDesc m_Desc{};
};

class INodeRenderPass
{
public:
    INodeRenderPass() = default;
    virtual ~INodeRenderPass() = default;

    virtual void PreRender() {};
    virtual void Render() = 0;
    virtual void PostRender() {};
};

class FrameGraph
{
public:
    static constexpr uint32_t MAX_RESOURCE_COUNT = 2048;
    static constexpr uint32_t MAX_RENDERPASS_NODE_COUNT = 1024;

public:
    FrameGraph()
        : m_ResourceCache{ MAX_RESOURCE_COUNT }
        , m_NodeCache{ MAX_RENDERPASS_NODE_COUNT }
    {}

    ~FrameGraph() = default;

    void Compile();
    void Execute();

    FrameGraphNodeHandle CreateNode(const FrameGraphNodeDesc& desc);

    void OutputGraphToMermaid(const std::string& filename);

private:
    // normally, it will be topologically sorted
    std::vector<FrameGraphNodeHandle> m_Nodes{};
    ObjectCache<std::string, FrameGraphResource> m_ResourceCache;
    ObjectCache<std::string, FrameGraphNode> m_NodeCache;

private:
    FrameGraphResourceHandle CreateInputResource(const FrameGraphResourceDesc& desc);
    FrameGraphResourceHandle CreateOutputResource(const FrameGraphResourceDesc& desc, FrameGraphNodeHandle producer);

    void CreateNodeDependents(FrameGraphNodeHandle node);
};
}

