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
    // 1) compute dependencies for each nodes
    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        CreateNodeDependents(nodeHandle);
    }

    // 2) top sort nodes using DFS
    std::stack<FrameGraphNodeHandle> nodeStack;

    std::vector<byte> visited(m_Nodes.size(), 0);
    std::vector<FrameGraphNodeHandle> sortedNodes;

    for (int n = 0; n < m_Nodes.size(); ++n)
    {
        if (visited[n])
            continue;

        nodeStack.push(m_Nodes[n]);

        while (!nodeStack.empty())
        {
            FrameGraphNodeHandle node = nodeStack.top();

            if (visited[node] == 2)
            {
                nodeStack.pop();
                continue;
            }

            if (visited[node] == 1)
            {
                sortedNodes.push_back(node);
                visited[node] = 2;
                continue;
            }

            visited[node] = 1;

            FrameGraphNode& nodeData = *m_NodeCache.GetPool().Access(node);

            for (FrameGraphNodeHandle dependent : nodeData.Dependents)
            {
                if (!visited[dependent])
                    nodeStack.push(dependent);
            }

        }
    }
    m_Nodes.clear();
    for (int i = sortedNodes.size() - 1; i >= 0; --i)
    {
        m_Nodes.push_back(sortedNodes[i]);
    }

    OutputGraphToMermaid("Profiling/framegraph.txt");

    // 3) create resources
    //std::list<SafePtr<Texture>> freeTextures;

    //for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    //{
    //    FrameGraphNode& node = *m_NodeCache.GetPool().Access(nodeHandle);
    //    for (FrameGraphResourceHandle outputResourceHandle : node.OutputResources)
    //    {
    //        FrameGraphResource& outputResource = *m_ResourceCache.GetPool().Access(outputResourceHandle);

    //        if (outputResource.Type == FrameGraphResourceType::eAttachment)
    //        {

    //        }
    //    }
    //}
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

void FrameGraph::CreateNodeDependents(FrameGraphNodeHandle node)
{
    FrameGraphNode& nodeData = *m_NodeCache.GetPool().Access(node);

    for (FrameGraphResourceHandle inputResourceHandle : nodeData.InputResources)
    {
        FrameGraphResource* inputResource = m_ResourceCache.GetPool().Access(inputResourceHandle);
        FrameGraphResource* associatedOutputResource = m_ResourceCache.Access(inputResource->Name);

        LNE_ASSERT(associatedOutputResource != nullptr, "Input resource has no associated output resource");

        inputResource->Producer = associatedOutputResource->Producer;
        inputResource->ProducerResourceHandle = associatedOutputResource->ProducerResourceHandle;
        inputResource->Info = associatedOutputResource->Info;

        ++associatedOutputResource->RefCount;

        FrameGraphNode& producerNode = *m_NodeCache.GetPool().Access(associatedOutputResource->Producer);
        producerNode.Dependents.push_back(node);
    }
}

FrameGraphNodeHandle FrameGraph::CreateNode(const FrameGraphNodeDesc& desc)
{
    FrameGraphNodeHandle handle = m_NodeCache.Insert(desc.Name);
    m_Nodes.emplace_back(handle);

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

void FrameGraph::OutputGraphToMermaid(const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Unable to open file for writing: " + filename);
    }

    file << "graph TD\n";

    // Add nodes and resources with styles
    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        FrameGraphNode& nodeData = *m_NodeCache.GetPool().Access(nodeHandle);

        // Add the node
        file << "    " << nodeData.Name << "[\"" << nodeData.Name << "\"]:::node\n";

        // Add input resources and arrows from resources to nodes
        for (FrameGraphResourceHandle inputHandle : nodeData.InputResources)
        {
            FrameGraphResource* inputResource = m_ResourceCache.GetPool().Access(inputHandle);
            std::string resourceType = std::string(FrameGraphResourceType::ToString(inputResource->Type));
            file << "    " << inputResource->Name << "[\"" << inputResource->Name
                << "<br/><i>(" << resourceType << ")</i>\"]:::resource\n";
            file << "    " << inputResource->Name << " -->|" << resourceType << "| " << nodeData.Name << "\n";
        }

        // Add output resources and arrows from nodes to resources
        for (FrameGraphResourceHandle outputHandle : nodeData.OutputResources)
        {
            FrameGraphResource* outputResource = m_ResourceCache.GetPool().Access(outputHandle);
            std::string resourceType = std::string(FrameGraphResourceType::ToString(outputResource->Type));
            file << "    " << outputResource->Name << "[\"" << outputResource->Name
                << "<br/><i>(" << resourceType << ")</i>\"]:::resource\n";
            file << "    " << nodeData.Name << " -->|" << resourceType << "| " << outputResource->Name << "\n";
        }
    }

    // Define styles
    file << "classDef node fill:#DB5C27,color:#333,stroke:#333,stroke-width:4px,rx:10px,ry:10px;\n";
    file << "classDef resource fill:#27DBC3,color:#333,stroke:#f66,stroke-width:2px,stroke-dasharray: 10 10;\n";
    file << "linkStyle default stroke:#000,stroke-width:2px;\n"; // Default link style
    file.close();
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
