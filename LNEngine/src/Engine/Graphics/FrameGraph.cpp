#include "lnepch.h"
#include "Texture.h"
#include "GfxContext.h"
#include "FrameGraph.h"

namespace lne
{
////////////////////////////////////////////////////////////////////
////// FrameGraph //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void FrameGraph::Compile()
{

}

void FrameGraph::Execute()
{

}

FrameGraphResourceHandle FrameGraph::CreateInputResource(const FrameGraphResourceDesc& desc)
{
    auto& pool = m_ResourceCache.GetPool();
    FrameGraphResourceHandle handle = pool.Allocate();

    if (handle == INVALID_OBJECT_POOL_HANDLE)
    {
        LNE_ERROR("Failed to create input resource");
        return handle;
    }

    FrameGraphResource& resource = *pool.Access(handle);

    resource.Name = desc.Name;
    resource.Type = desc.Type;
    resource.Producer = INVALID_OBJECT_POOL_HANDLE;
    resource.ProducerResourceHandle = INVALID_OBJECT_POOL_HANDLE; // to be set later at compile time

    return handle;
}

FrameGraphResourceHandle FrameGraph::CreateOutputResource(const FrameGraphResourceDesc& desc, FrameGraphNodeHandle producer)
{
    auto& pool = m_ResourceCache.GetPool();
    FrameGraphResourceHandle handle = INVALID_OBJECT_POOL_HANDLE;

    if (desc.Type == FrameGraphResourceType::eProxy)
        handle = pool.Allocate();
    else
        handle = m_ResourceCache.Insert(desc.Name);

    if (handle == INVALID_OBJECT_POOL_HANDLE)
    {
        LNE_ERROR("Failed to create output resource");
        return handle;
    }

    FrameGraphResource& resource = *pool.Access(handle);

    resource.Name = desc.Name;
    resource.Type = desc.Type;

    if (desc.Type != FrameGraphResourceType::eProxy)
    {
        resource.Info = desc.Info;
        resource.Producer = producer;
        resource.ProducerResourceHandle = handle;
    }

    return handle;
}

FrameGraphNodeHandle FrameGraph::CreateNode(const FrameGraphNodeDesc& desc)
{
    FrameGraphNodeHandle handle = m_NodeCache.Insert(desc.Name);

    if (handle == INVALID_OBJECT_POOL_HANDLE)
    {
        LNE_ERROR("Failed to create node");
        return handle;
    }

    FrameGraphNode& node = *m_NodeCache.GetPool().Access(handle);
    node.Enabled = desc.Enabled;
    node.Name = desc.Name;
    // node.Framebuffer is created at compile time

    for (const auto& inputResource : desc.InputResources)
    {
        FrameGraphResourceHandle inputHandle = CreateInputResource(inputResource);
        node.InputResources.push_back(inputHandle);
    }

    for (const auto& outputResource : desc.OutputResources)
    {
        FrameGraphResourceHandle outputHandle = CreateOutputResource(outputResource, handle);
        node.OutputResources.push_back(outputHandle);
    }

    return handle;
}

////////////////////////////////////////////////////////////////////
////// FrameGraphResourceDescBuilder ///////////////////////////////
////////////////////////////////////////////////////////////////////

FrameGraphResourceDescBuilder& FrameGraphResourceDescBuilder::SetImageDimension(uint32_t width, uint32_t height, uint32_t depth)
{
    m_Extent.width = width;
    m_Extent.height = height;
    m_Extent.depth = depth;
    return *this;
}

FrameGraphResourceDescBuilder& FrameGraphResourceDescBuilder::SetDefaultColorAttachmentInfos()
{
    m_ImageInfo.Format = vk::Format::eR8G8B8A8Unorm;
    m_ImageInfo.Flags = vk::ImageUsageFlagBits::eColorAttachment;
    m_ImageInfo.Aspect = vk::ImageAspectFlagBits::eColor;
    m_ImageInfo.LoadOp = vk::AttachmentLoadOp::eClear;

    return *this;
}

FrameGraphResourceDescBuilder& FrameGraphResourceDescBuilder::SetDefaultDepthAttachmentInfos()
{
    m_ImageInfo.Format = vk::Format::eD32Sfloat;
    m_ImageInfo.Flags = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    m_ImageInfo.Aspect = vk::ImageAspectFlagBits::eDepth;
    m_ImageInfo.LoadOp = vk::AttachmentLoadOp::eClear;

    return *this;
}

FrameGraphResourceDesc FrameGraphResourceDescBuilder::Build()
{
    if (m_Desc.Name.empty())
        LNE_ERROR("Resource name is empty");
    
    switch (m_Desc.Type)
    {
    case FrameGraphResourceType::eBuffer:
        if (m_Desc.Info.Buffer.Size == 0)
            LNE_ERROR("Buffer size is 0");
        m_Desc.Info.Buffer = m_BufferInfo;
        break;
    case FrameGraphResourceType::eAttachment:
    case FrameGraphResourceType::eTexture:
        if (m_Desc.Info.Image.Extent.width == 0 || m_Desc.Info.Image.Extent.height == 0)
            LNE_ERROR("Image width or height is 0");
        m_Desc.Info.Image = m_ImageInfo;
        m_Desc.Info.Image.Extent = m_Extent;
        break;
    default:
        LNE_ERROR("Unknown resource type");
        break;
    }

    return m_Desc;
}

////////////////////////////////////////////////////////////////////
////// FrameGraphNodeDescBuilder ///////////////////////////////////
////////////////////////////////////////////////////////////////////

FrameGraphNodeDesc FrameGraphNodeDescBuilder::Build()
{
    if (m_Desc.Name.empty())
        LNE_ERROR("Node name is empty");

    if (m_Desc.InputResources.empty() && m_Desc.OutputResources.empty())
        LNE_ERROR("Node has no input or output resources");

    return m_Desc;
}
}
