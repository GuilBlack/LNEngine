#include "Graphics/Resources/Texture.h"
#include "Graphics/GfxContext.h"
#include "FrameGraph.h"
#include "Core/ApplicationBase.h"
#include "Core/Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Resources/Mesh.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/DynamicDescriptorAllocator.h"
#include "Scene/Components.h"
#include "RenderPass/RenderPass.h"
#include "Core/Utils/Profiling.h"
#include "../CommandPoolManager.h"

namespace lne
{
////////////////////////////////////////////////////////////////////
////// FrameGraph //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
#define PROFILING_COL 0xFFF43E
FrameGraph::FrameGraph()
    : m_ResourceCache{ MAX_RESOURCE_COUNT }
    , m_NodeCache{ MAX_RENDERPASS_NODE_COUNT }
{
    m_Context = ApplicationBase::GetWindow().GetGfxContext();
}

FrameGraph::FrameGraph(const std::string& name)
    : m_Name(name)
    , m_ResourceCache{ MAX_RESOURCE_COUNT }
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
                auto& imageInfo = std::get<FrameGraphResourceImageInfo>(outputResource.Info.Variant);
                bool isDepth = (imageInfo.Format == vk::Format::eD16Unorm
                    || imageInfo.Format == vk::Format::eD32Sfloat
                    || imageInfo.Format == vk::Format::eD16UnormS8Uint
                    || imageInfo.Format == vk::Format::eD24UnormS8Uint
                    || imageInfo.Format == vk::Format::eD32SfloatS8Uint);

                if (isDepth)
                {
                    outputResource.Resource = Texture::CreateDepthTexture(m_Context,
                        imageInfo.Extent.width, imageInfo.Extent.height, TextureUsageType::eSampled, outputResource.Name);
                    break;
                }
                outputResource.Resource = Texture::CreateColorAttachmentTexture(m_Context,
                    imageInfo.Extent.width, imageInfo.Extent.height,
                    imageInfo.Format, TextureUsageType::eSampled, outputResource.Name);
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

            if (associatedOutputResource->Type == FrameGraphResourceType::eProxy)
                associatedOutputResource = GetProxyRealResource(associatedOutputResource);

            --associatedOutputResource->RefCount;

            if (associatedOutputResource->RefCount != 0 || associatedOutputResource->Info.External)
                continue;

            switch (associatedOutputResource->Type)
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

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, WorldRenderer* worldRenderer)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    // init the command buffers for each nodes
    auto& renderer = ApplicationBase::GetRenderer();


    std::vector<vk::CommandBuffer> secondaryCommandBuffers{};
    secondaryCommandBuffers.resize(m_Nodes.size());

    //enki::TaskSet set(
    //    (u32)m_Nodes.size(),
    //    [this, &secondaryCommandBuffers, worldRenderer](enki::TaskSetPartition range, u32 threadnum)
    //    {
    //        u32 currentFrameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndex();
    //        for (u32 i = range.start; i < range.end; ++i)
    //        {
    //            auto& commandPoolManager = m_Context->GetCommandPoolManager();
    //            FrameGraphNode* node = m_NodeCache.GetPool().Access(m_Nodes[i]);

    //            if (node->Enabled == false)
    //                continue;

    //            vk::CommandBuffer cmdBuffer{};
    //            if (node->Type == RenderPassType::eGraphics)
    //                cmdBuffer = commandPoolManager.BeginRenderPassCommandBuffer(currentFrameIndex, &node->Framebuffer);
    //            else
    //                cmdBuffer = commandPoolManager.BeginRenderPassCommandBuffer(currentFrameIndex);

    //            // do render pass here
    //            node->RenderPass->Execute(cmdBuffer, worldRenderer, this, node);

    //            cmdBuffer.end();
    //            secondaryCommandBuffers[i] = cmdBuffer;
    //        }
    //    }
    //);

    //set.m_Priority = enki::TASK_PRIORITY_MED;
    //ApplicationBase::GetTaskScheduler()->AddTaskSetToPipe(&set);
    //ApplicationBase::GetTaskScheduler()->WaitforTask(&set);

    for (u32 i = 0; i < m_Nodes.size(); ++i)
    {
        FrameGraphNodeHandle nodeHandle = m_Nodes[i];
        FrameGraphNode* node = m_NodeCache.GetPool().Access(nodeHandle);

        if (!node->Enabled)
            continue;

        const std::string scopeName = "Execute Render Pass: " + node->Name;
        LNE_PROFILE_SCOPE_STR_C(scopeName, PROFILING_COL)
        renderer.PushLabel(commandBuffer, node->Name);

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
            default:
                break;
            }
        }
        if (node->Type == RenderPassType::eGraphics)
        {
            vk::Extent3D extent = node->Framebuffer.GetExtent();
            vk::Viewport viewport = { 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
            viewport.y += viewport.height;
            viewport.height *= -1;
            commandBuffer.setViewport(0, viewport);
            vk::Rect2D scissor = { {0, 0}, vk::Extent2D{ extent.width, extent.height } };
            commandBuffer.setScissor(0, scissor);
        }

        LNE_ASSERT(node->RenderPass != nullptr, "Node has no render pass");

        node->RenderPass->PreExecute(commandBuffer, this, node);
        if (node->Type == RenderPassType::eGraphics)
            node->Framebuffer.Bind(commandBuffer);

        //commandBuffer.executeCommands(secondaryCommandBuffers[i]);

        node->RenderPass->Execute(commandBuffer, worldRenderer, this, node);
        
        if (node->Type == RenderPassType::eGraphics)
            node->Framebuffer.Unbind(commandBuffer);
        node->RenderPass->PostExecute(commandBuffer, this, node);

        renderer.PopLabel(commandBuffer);
    }
}

void FrameGraph::OnResize(WindowResizeEvent& e)
{
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
                auto& imageInfo = std::get<FrameGraphResourceImageInfo>(outputResource.Info.Variant);
                bool isDepth = (imageInfo.Format == vk::Format::eD16Unorm
                    || imageInfo.Format == vk::Format::eD32Sfloat
                    || imageInfo.Format == vk::Format::eD16UnormS8Uint
                    || imageInfo.Format == vk::Format::eD24UnormS8Uint
                    || imageInfo.Format == vk::Format::eD32SfloatS8Uint);

                imageInfo.Extent.width = e.GetWidth();
                imageInfo.Extent.height = e.GetHeight();

                if (isDepth)
                {
                    outputResource.Resource = Texture::CreateDepthTexture(
                        m_Context, e.GetWidth(), e.GetHeight(),
                        TextureUsageType::eSampled, outputResource.Name);
                    break;
                }
                outputResource.Resource = Texture::CreateColorAttachmentTexture(
                    m_Context, e.GetWidth(), e.GetHeight(),
                    imageInfo.Format, TextureUsageType::eSampled, outputResource.Name);
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

            if (associatedOutputResource->Type == FrameGraphResourceType::eProxy)
                associatedOutputResource = GetProxyRealResource(associatedOutputResource);

            --associatedOutputResource->RefCount;

            switch (associatedOutputResource->Type)
            {
            case FrameGraphResourceType::eAttachment:
            case FrameGraphResourceType::eTexture:
            {
                const auto& outputImageInfo = std::get<FrameGraphResourceImageInfo>(associatedOutputResource->Info.Variant);
                auto& imageInfo = std::get<FrameGraphResourceImageInfo>(inputResource.Info.Variant);
                imageInfo.Extent.width = e.GetWidth();
                imageInfo.Extent.height = e.GetHeight();

                if (associatedOutputResource->RefCount != 0 || associatedOutputResource->Info.External)
                    continue;

                freeTextures.push_back(associatedOutputResource->Resource.GetAs<Texture>());
                break;
            }
            case FrameGraphResourceType::eBuffer:
            {
                LNE_ASSERT(false, "Buffer resource not implemented yet");
                if (associatedOutputResource->RefCount != 0 || associatedOutputResource->Info.External)
                    continue;
                break;
            }
            }
        }
    }

    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
        CreateFramebuffers(nodeHandle);

    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        FrameGraphNode* node = m_NodeCache.GetPool().Access(nodeHandle);
        node->RenderPass->OnResize(this, node);
    }
}

void FrameGraph::RenderImGui()
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    ImGui::Begin("Frame Graph Render Passes");

    for (FrameGraphNodeHandle nodeHandle : m_Nodes)
    {
        FrameGraphNode* node = m_NodeCache.GetPool().Access(nodeHandle);
        if (ImGui::CollapsingHeader(node->Name.c_str()))
        {
            node->RenderPass->OnImGuiRender();
        }
    }

    ImGui::End();
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

    handle = m_ResourceCache.Insert(desc.Name);

    if (handle == INVALID_OBJECT_POOL_HANDLE)
    {
        LNE_ERROR("Failed to create output resource");
        return handle;
    }

    FrameGraphResource& resource = *pool.Access(handle);

    resource.Name = desc.Name;
    resource.Type = desc.Type;
    resource.Info = desc.Info;
    resource.Producer = producer;
    resource.ProducerResourceHandle = handle;

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

        if (associatedOutputResource->Type == FrameGraphResourceType::eProxy)
            inputResource->Info = GetProxyRealResourceInfo(associatedOutputResource);
        else
        {
            inputResource->Info = associatedOutputResource->Info;
            ++associatedOutputResource->RefCount;
        }

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
            auto& imageInfo = std::get<FrameGraphResourceImageInfo>(outputResource.Info.Variant);
            vk::Extent3D attachmentExtent = imageInfo.Extent;
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
                imageInfo.LoadOp,
                vk::AttachmentStoreOp::eStore,
                attachmentLayout,
                attachmentLayout,
                vk::ClearValue().setColor(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f })
                    .setDepthStencil({ 0.0f, 0 })
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

        if (associatedOutputResource->Type == FrameGraphResourceType::eProxy)
            associatedOutputResource = GetProxyRealResource(associatedOutputResource);

        if (associatedOutputResource->Info.External)
            continue;

        switch (inputResource.Type)
        {
        case FrameGraphResourceType::eAttachment:
        case FrameGraphResourceType::eTexture:
        {
            auto& imageInfo = std::get<FrameGraphResourceImageInfo>(inputResource.Info.Variant);
            vk::Extent3D attachmentExtent = imageInfo.Extent;
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
                    .setDepthStencil({ 0.0f, 0 })
            };

            if (inputResource.Type == FrameGraphResourceType::eTexture)
                break;

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

FrameGraphResourceInfo FrameGraph::GetProxyRealResourceInfo(FrameGraphResource* resource)
{
    FrameGraphResourceInfo info = resource->Info;
    while (resource->Type == FrameGraphResourceType::eProxy)
    {
        const auto& proxyInfo = std::get<FrameGraphResourceProxyInfo>(resource->Info.Variant);
        resource = m_ResourceCache.Access(proxyInfo.OriginalName);
        LNE_ASSERT(resource != nullptr, "Proxy resource has no original resource");
    }
    ++resource->RefCount;
    return resource->Info;
}

FrameGraphResource* FrameGraph::GetProxyRealResource(FrameGraphResource* resource)
{
    while (resource->Type == FrameGraphResourceType::eProxy)
    {
        const auto& proxyInfo = std::get<FrameGraphResourceProxyInfo>(resource->Info.Variant);
        resource = m_ResourceCache.Access(proxyInfo.OriginalName);
        LNE_ASSERT(resource != nullptr, "Proxy resource has no original resource");
    }
    return resource;
}

void FrameGraph::SortGraph(std::vector<FrameGraphNodeHandle>& nodes)
{
    std::stack<FrameGraphNodeHandle> nodeStack;

    std::vector<u8> visited(nodes.size(), 0);
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

    OutputGraphToMermaid();
}

void FrameGraph::BindRenderPass(SafePtr<RenderPass> renderPass)
{
    FrameGraphNode* node = m_NodeCache.Access(std::string(renderPass->GetName()));

    LNE_ASSERT(node != nullptr, "Node not found");

    node->RenderPass = renderPass;
    renderPass->OnBindInternal(this, node);
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
    node.Type = desc.Type;
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

void FrameGraph::OutputGraphToMermaid()
{
    namespace fs = std::filesystem;

    fs::path workingDir = fs::current_path();
    std::string fileName = m_Name + ".mmd";
    fs::path filePath = workingDir / "Profiling" / fileName;

    if (!fs::exists(workingDir / "Profiling"))
        fs::create_directory(workingDir / "Profiling");
    
    std::ofstream file(filePath);
    if (!file.is_open())
        return;

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

std::vector<SafePtr<RenderPass>> FrameGraph::GetRenderPassesWithSignature(EntitySignature signature)
{
    std::vector<SafePtr<RenderPass>> renderPasses;
    for (auto nodeHandle : m_Nodes)
    {
        SafePtr<RenderPass> renderPass = m_NodeCache.GetPool().Access(nodeHandle)->RenderPass;
        const EntitySignature& renderPassSignature = renderPass->MustHaveComponents();
        if ((renderPassSignature & signature) == renderPassSignature)
            renderPasses.push_back(renderPass);
    }

    return renderPasses;
}

////////////////////////////////////////////////////////////////////
////// FrameGraphResourceDescBuilder ///////////////////////////////
////////////////////////////////////////////////////////////////////

FrameGraphResourceDescBuilder& FrameGraphResourceDescBuilder::SetImageDimension(u32 width, u32 height, u32 depth)
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
        LNE_ERROR("Resource scopeName is empty");
    
    switch (m_Desc.Type)
    {
    case FrameGraphResourceType::eBuffer:
    {
        if (m_BufferInfo.Size == 0)
            LNE_ERROR("Buffer size is 0");
        m_Desc.Info.Variant = m_BufferInfo;
        break;
    }
    case FrameGraphResourceType::eAttachment:
    case FrameGraphResourceType::eTexture:
    {
        if (m_Extent == vk::Extent3D(0, 0, 0))
            LNE_ERROR("Image width or height or height is 0");
        m_ImageInfo.Extent = m_Extent;
        m_Desc.Info.Variant = m_ImageInfo;
        break;
    }
    case FrameGraphResourceType::eProxy:
    {
        if (m_ProxyInfo.OriginalName.empty())
            LNE_ERROR("Proxy resource has no original resource scopeName");
        m_Desc.Info.Variant = m_ProxyInfo;
        break;
    }
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
        LNE_ERROR("Node scopeName is empty");

    if (m_Desc.InputResources.empty() && m_Desc.OutputResources.empty())
        LNE_ERROR("Node has no input or output resources");

    return m_Desc;
}

void RenderPassTask::ExecuteRange(enki::TaskSetPartition range, u32 threadnum)
{
    for (u32 i = range.start; i < range.end; ++i)
    {
    }
}

}
