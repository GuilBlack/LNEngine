#include "lnepch.h"
#include "Texture.h"
#include "GfxContext.h"
#include "FrameGraph.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "DynamicDescriptorAllocator.h"

namespace lne
{
////////////////////////////////////////////////////////////////////
////// FrameGraph //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

FrameGraph::FrameGraph()
    : m_ResourceCache{ MAX_RESOURCE_COUNT }
    , m_NodeCache{ MAX_RENDERPASS_NODE_COUNT }
{
    m_Context = ApplicationBase::GetWindow().GetGfxContext();
}

void FrameGraph::Compile()
{
    // 1) compute dependencies for each nodes
    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        CreateNodeDependents(nodeHandle);
    }

    // 2) top sort nodes using DFS
    SortGraph(m_Nodes);

    // 3) create resources
    std::list<SafePtr<Texture>> freeTextures;

    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        FrameGraphNode& node = *m_NodeCache.GetPool().Access(nodeHandle);
        for (FrameGraphResourceHandle outputResourceHandle : node.OutputResources)
        {
            FrameGraphResource& outputResource = *m_ResourceCache.GetPool().Access(outputResourceHandle);

            if (outputResource.Info.External)
                continue;

            switch (outputResource.Type)
            {
            case FrameGraphResourceType::eAttachment:
            {
                // should implement resource aliasing later
                bool isDepth = (outputResource.Info.Image.Format == vk::Format::eD16Unorm
                    || outputResource.Info.Image.Format == vk::Format::eD32Sfloat
                    || outputResource.Info.Image.Format == vk::Format::eD16UnormS8Uint
                    || outputResource.Info.Image.Format == vk::Format::eD24UnormS8Uint
                    || outputResource.Info.Image.Format == vk::Format::eD32SfloatS8Uint);

                if (isDepth)
                {
                    outputResource.Resource = Texture::CreateDepthTexture(m_Context,
                        outputResource.Info.Image.Extent.width, outputResource.Info.Image.Extent.height, outputResource.Name);
                    break;
                }
                outputResource.Resource = Texture::CreateColorAttachmentTexture(m_Context,
                    outputResource.Info.Image.Extent.width, outputResource.Info.Image.Extent.height,
                    outputResource.Info.Image.Format, outputResource.Name);
                break;
            }
            case FrameGraphResourceType::eBuffer:
            {
                LNE_ASSERT(false, "Buffer resource not implemented yet");
                break;
            }
            }
        }

        for (FrameGraphResourceHandle inputResourceHandle : node.InputResources)
        {
            FrameGraphResource& inputResource = *m_ResourceCache.GetPool().Access(inputResourceHandle);
            FrameGraphResource* associatedOutputResource = m_ResourceCache.Access(inputResource.Name);

            LNE_ASSERT(associatedOutputResource != nullptr, "Input resource has no associated output resource");

            --associatedOutputResource->RefCount;

            if (associatedOutputResource->RefCount != 0 || associatedOutputResource->Info.External)
                continue;

            switch (inputResource.Type)
            {
            case FrameGraphResourceType::eAttachment:
            case FrameGraphResourceType::eTexture:
            {
                freeTextures.push_back(associatedOutputResource->Resource.GetAs<Texture>());
                break;
            }
            case FrameGraphResourceType::eBuffer:
            {
                LNE_ASSERT(false, "Buffer resource not implemented yet");
                break;
            }
            }
        }
    }

    // 4) create framebuffers
    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        CreateFramebuffers(nodeHandle);
    }
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer)
{
    // traverse nodes in topological order
    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        FrameGraphNode* node = m_NodeCache.GetPool().Access(nodeHandle);

        if (!node->Enabled)
            continue;

        node->RenderPass->PreRender(commandBuffer, node);

        for (FrameGraphResourceHandle inputResourceHandle : node->InputResources)
        {
            FrameGraphResource* inputResource = m_ResourceCache.GetPool().Access(inputResourceHandle);

            switch (inputResource->Type)
            {
            case FrameGraphResourceType::eTexture:
            {
                SafePtr<Texture> texture = inputResource->Resource.GetAs<Texture>();
                texture->TransitionLayout(commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                break;
            }
            }
        }
        vk::Extent3D extent = node->Framebuffer.GetExtent();
        vk::Viewport viewport = { 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
        viewport.y += viewport.height;
        viewport.height *= -1;
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor = { {0, 0}, vk::Extent2D{ extent.width, extent.height } };
        commandBuffer.setScissor(0, scissor);

        LNE_ASSERT(node->RenderPass != nullptr, "Node has no render pass");

        node->Framebuffer.Bind(commandBuffer);

        node->RenderPass->Render(commandBuffer, node);

        node->Framebuffer.Unbind(commandBuffer);

        node->RenderPass->PostRender(commandBuffer, node);
    }
}

void FrameGraph::OnResize(WindowResizeEvent& e)
{}

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

void FrameGraph::CreateFramebuffers(FrameGraphNodeHandle nodeHandle)
{
    FrameGraphNode& node = *m_NodeCache.GetPool().Access(nodeHandle);

    std::vector<AttachmentDesc> colorAttachments;
    AttachmentDesc depthAttachment;

    vk::Extent2D extent = { 0, 0 };

    for (FrameGraphResourceHandle outputResourceHandle : node.OutputResources)
    {
        FrameGraphResource& outputResource = *m_ResourceCache.GetPool().Access(outputResourceHandle);

        if (outputResource.Info.External)
            continue;

        switch (outputResource.Type)
        {
        case FrameGraphResourceType::eAttachment:
        {
            vk::Extent3D attachmentExtent = outputResource.Info.Image.Extent;
            if (extent.width == 0)
                extent.width = attachmentExtent.width;
            else
                LNE_ASSERT(extent.width == attachmentExtent.width, "Inconsistent attachment width");

            if (extent.height == 0)
                extent.height = attachmentExtent.height;
            else
                LNE_ASSERT(extent.height == attachmentExtent.height, "Inconsistent attachment height");

            SafePtr<Texture> texture = outputResource.Resource.GetAs<Texture>();

            vk::ImageLayout attachmentLayout = texture->IsDepth()
                ? vk::ImageLayout::eDepthStencilAttachmentOptimal
                : vk::ImageLayout::eColorAttachmentOptimal;

            auto attachmentDesc = AttachmentDesc{
                texture,
                outputResource.Info.Image.LoadOp,
                vk::AttachmentStoreOp::eStore,
                attachmentLayout,
                attachmentLayout,
                vk::ClearValue().setColor(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f })
                    .setDepthStencil({ 1.0f, 0 })
            };
            if (texture->IsDepth())
                depthAttachment = attachmentDesc;
            else
                colorAttachments.push_back(attachmentDesc);
            break;
        }
        }
    }

    for (FrameGraphResourceHandle inputResourceHandle : node.InputResources)
    {
        FrameGraphResource& inputResource = *m_ResourceCache.GetPool().Access(inputResourceHandle);
        FrameGraphResource* associatedOutputResource = m_ResourceCache.Access(inputResource.Name);
        LNE_ASSERT(associatedOutputResource != nullptr, "Input resource has no associated output resource");

        if (associatedOutputResource->Info.External)
            continue;

        switch (inputResource.Type)
        {
        case FrameGraphResourceType::eAttachment:
        case FrameGraphResourceType::eTexture:
        {
            vk::Extent3D attachmentExtent = inputResource.Info.Image.Extent;
            if (extent.width == 0)
                extent.width = attachmentExtent.width;
            else
                LNE_ASSERT(extent.width == attachmentExtent.width, "Inconsistent attachment width");

            if (extent.height == 0)
                extent.height = attachmentExtent.height;
            else
                LNE_ASSERT(extent.height == attachmentExtent.height, "Inconsistent attachment height");

            inputResource.Resource = associatedOutputResource->Resource;

            SafePtr<Texture> texture = associatedOutputResource->Resource.GetAs<Texture>();

            vk::ImageLayout attachmentLayout = texture->IsDepth()
                ? vk::ImageLayout::eDepthStencilAttachmentOptimal
                : vk::ImageLayout::eColorAttachmentOptimal;

            auto attachmentDesc = AttachmentDesc{
                texture,
                vk::AttachmentLoadOp::eLoad,
                vk::AttachmentStoreOp::eStore,
                attachmentLayout,
                attachmentLayout,
                vk::ClearValue().setColor(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f })
                    .setDepthStencil({ 1.0f, 0 })
            };
            if (texture->IsDepth())
                depthAttachment = attachmentDesc;
            else
                colorAttachments.push_back(attachmentDesc);
            break;
        }
        }
    }
    node.Framebuffer = Framebuffer(m_Context, colorAttachments, depthAttachment);
}

void FrameGraph::SortGraph(std::vector<FrameGraphNodeHandle>& nodes)
{
    std::stack<FrameGraphNodeHandle> nodeStack;

    std::vector<byte> visited(nodes.size(), 0);
    std::vector<FrameGraphNodeHandle> sortedNodes;

    for (int n = 0; n < nodes.size(); ++n)
    {
        if (visited[n])
            continue;

        nodeStack.push(nodes[n]);

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
    nodes.clear();
    for (int i = (int)(sortedNodes.size() - 1); i >= 0; --i)
    {
        nodes.push_back(sortedNodes[i]);
    }

    OutputGraphToMermaid("Profiling/framegraph.txt");
}

void FrameGraph::BindRenderPass(SafePtr<IRenderPass> renderPass)
{
    FrameGraphNode* node = m_NodeCache.Access(std::string(renderPass->GetName()));

    LNE_ASSERT(node != nullptr, "Node not found");

    node->RenderPass = renderPass;
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
    m_ImageInfo.Format = vk::Format::eB8G8R8A8Unorm;
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
        if (m_Extent == 0 || m_Extent == 0)
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
