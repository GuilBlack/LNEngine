#include "Renderer.h"
#include "Core/Utils/Log.h"
#include "Engine/Core/Window.h"
#include "GfxContext.h"
#include "CommandBufferManager.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Graphics/Pipeline.h"
#include "Core/Utils/Defines.h"
#include "DynamicDescriptorAllocator.h"
#include "Mesh.h"
#include "StorageBuffer.h"
#include "Scene/Components.h"
#include "Material.h"

// TODO: move this to a resource manager
#include <stb/stb_image.h>

namespace lne
{
void Renderer::Init(std::unique_ptr<Window>& window)
{
    m_Context = window->GetGfxContext();
    m_Swapchain = window->GetSwapchain();
    m_GraphicsCommandBufferManager = std::make_unique<CommandBufferManager>(m_Context.GetPtr(), m_Swapchain->GetImageCount(), EQueueFamilyType::Graphics);

    for (uint32_t i = 0; i < m_Swapchain->GetImageCount(); i++)
    {
        InitFrameData(i);
    }
}

void Renderer::Nuke()
{
    m_Context->WaitIdle();
    for (auto& frameData : m_FrameData)
    {
        frameData.GlobalUniforms.Destroy();
        frameData.DescriptorAllocator.Reset();
        m_Context->GetDevice().destroyDescriptorSetLayout(frameData.DescriptorSetLayout);
    }
    m_FrameData.clear();
    m_GraphicsCommandBufferManager.reset();
    m_Context.Reset();
    m_Swapchain.Reset();
}

void Renderer::BeginFrame()
{
    uint32_t imageIndex = m_Swapchain->GetCurrentFrameIndex();
    m_GraphicsCommandBufferManager->StartCommandBuffer(imageIndex);
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer(), vk::ImageLayout::eGeneral);
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();

    auto viewport = m_Swapchain->GetViewport();
    cmdBuffer.setScissor(0, viewport.GetScissor());
    auto vp = viewport.GetViewport();
    vp.y += vp.height;
    vp.height *= -1;
    cmdBuffer.setViewport(0, viewport.GetViewport());

    m_FrameData[imageIndex].DescriptorAllocator->Clear();

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), viewport.GetExtent().width / (float)viewport.GetExtent().height, 0.1f, 10.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    GlobalUniforms uniforms = {
        .ViewProj = proj * view,
        .View = view,
        .Proj = proj
    };
    
    m_FrameData[imageIndex].GlobalUniforms.CopyData(cmdBuffer, uniforms);
    m_FrameData[imageIndex].DescriptorSet = m_FrameData[imageIndex].DescriptorAllocator->Allocate(m_FrameData[imageIndex].DescriptorSetLayout);

    auto bufferInfo = m_FrameData[imageIndex].GlobalUniforms.GetDescriptorInfo();
    vk::WriteDescriptorSet writeDescriptorSet = vk::WriteDescriptorSet{
            m_FrameData[imageIndex].DescriptorSet,
            0,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &bufferInfo,
            nullptr
    };

    writeDescriptorSet.dstSet = m_FrameData[imageIndex].DescriptorSet;
    writeDescriptorSet.dstBinding = 0;

    m_Context->GetDevice().updateDescriptorSets(writeDescriptorSet, nullptr);
}

void Renderer::EndFrame()
{
    auto currentImage = m_Swapchain->GetCurrentImage();
    const vk::CommandBuffer& cb = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    currentImage->TransitionLayout(cb, vk::ImageLayout::ePresentSrcKHR);

    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::SubmitInfo submitInfo = m_Swapchain->GetSubmitInfo(waitStages);
    m_GraphicsCommandBufferManager->Submit(submitInfo);
}

void Renderer::BeginRenderPass(const Framebuffer& framebuffer) const
{
    framebuffer.Bind(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer());
}

void Renderer::EndRenderPass(const Framebuffer& framebuffer) const
{
    framebuffer.Unbind(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer());
}

void Renderer::Draw(SafePtr<Material> material, struct Geometry& geometry, TransformComponent& objTransform)
{
    auto pipeline = material->GetPipeline();
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    pipeline->Bind(cmdBuffer);
    
    // Create & update geometry descriptor set
    auto geometryDescSetLayout = pipeline->GetDescriptorSetLayouts()[1];
    vk::DescriptorSet geometryDescSet = m_FrameData[m_Swapchain->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(geometryDescSetLayout);

    auto vertexInfo = geometry.VertexGPUBuffer->GetDescriptorInfo();
    auto indexInfo = geometry.IndexGPUBuffer->GetDescriptorInfo();
    std::vector<vk::WriteDescriptorSet> writeGeoDescriptorSets;
    writeGeoDescriptorSets.emplace_back(vk::WriteDescriptorSet{
        geometryDescSet,
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &vertexInfo,
        nullptr
    });
    writeGeoDescriptorSets.emplace_back(vk::WriteDescriptorSet{
        geometryDescSet,
        1,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &indexInfo,
        nullptr
    });

    m_Context->GetDevice().updateDescriptorSets(writeGeoDescriptorSets, nullptr);

    // Create & update object descriptor set
    objTransform.UniformBuffers->CopyData(cmdBuffer, objTransform.GetModelMatrix());
    auto objDescSetLayout = pipeline->GetDescriptorSetLayouts()[2];
    vk::DescriptorSet objDescSet = m_FrameData[m_Swapchain->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(objDescSetLayout);

    auto objInfo = objTransform.UniformBuffers->GetCurrentBuffer().GetDescriptorInfo();
    vk::WriteDescriptorSet writeObjDescriptorSet = vk::WriteDescriptorSet{
        objDescSet,
        0,
        0,
        1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        &objInfo,
        nullptr
    };
    m_Context->GetDevice().updateDescriptorSets(writeObjDescriptorSet, nullptr);

    std::vector<vk::WriteDescriptorSet> matWriteDescriptorSets;
    std::vector<vk::DescriptorBufferInfo> matUbInfo;
    matUbInfo.reserve(material->m_UniformBuffers.size());
    vk::DescriptorSet matDescSet = m_FrameData[m_Swapchain->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[3]);
    for (const auto& [binding, ub] : material->m_UniformBuffers)
    {
        matUbInfo.emplace_back(ub.GetDescriptorInfo());
        matWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            matDescSet,
            binding,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &matUbInfo.back(),
            nullptr
        });
    }
    m_Context->GetDevice().updateDescriptorSets(matWriteDescriptorSets, nullptr);

    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0, { m_FrameData[m_Swapchain->GetCurrentFrameIndex()].DescriptorSet, geometryDescSet, objDescSet, matDescSet, m_Context->GetBindlessDescriptorSet() }, {});
    cmdBuffer.draw(geometry.IndexCount, 1, 0, 0);
}

SafePtr<GfxPipeline> Renderer::CreateGraphicsPipeline(const GraphicsPipelineDesc& createInfo)
{
    SafePtr<GfxPipeline> pipeline;
    pipeline.Reset(lnnew GfxPipeline(m_Context, createInfo));
    return pipeline;
}

SafePtr<class StorageBuffer> Renderer::CreateGeometryBuffer(const void* data, size_t size)
{
    SafePtr<StorageBuffer> buffer;
    buffer.Reset(lnnew StorageBuffer(m_Context, (uint64_t)size, data));
    return buffer;
}

SafePtr<Texture> Renderer::CreateTexture(const std::string& fullPath)
{
    if (std::filesystem::exists(fullPath) == false)
    {
        LNE_ERROR("Texture file not found: {0}", fullPath);
        return SafePtr<Texture>();
    }

    // load image to binary
    int texWidth, texHeight, texChannels;
    uint8_t* pixels = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    
    if (!pixels)
    {
        LNE_ERROR("Failed to load texture image: {0}", fullPath);
        return SafePtr<Texture>();
    }
    std::filesystem::path fsFullPath = fullPath;

    SafePtr<Texture> texture = Texture::CreateColorTexture2D(m_Context, texWidth, texHeight, true, std::format("Texture: {}", fsFullPath.filename().string()));

    texture->UploadData(pixels);

    stbi_image_free(pixels);

    return texture;
}

SafePtr<UniformBufferManager> Renderer::RegisterObject()
{
    SafePtr<UniformBufferManager> uboManager;
    uboManager.Reset(lnnew UniformBufferManager(m_Context, sizeof(glm::mat4)));
    return uboManager;
}

void Renderer::InitFrameData(uint32_t index)
{
    m_FrameData.emplace_back(
            UniformBuffer(m_Context, sizeof(GlobalUniforms)),
            SafePtr(lnnew DynamicDescriptorAllocator(m_Context, 
                { 
                    { vk::DescriptorType::eUniformBuffer, 512 },
                    { vk::DescriptorType::eStorageBuffer, 512 }
                }, 
                "GlobalDescAlloc" + std::to_string(index), 1)),
            m_Context->CreateDescriptorSetLayout({
                vk::DescriptorSetLayoutBinding{
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    1,
                    vk::ShaderStageFlagBits::eAllGraphics
                }
            })
        );
}
}
