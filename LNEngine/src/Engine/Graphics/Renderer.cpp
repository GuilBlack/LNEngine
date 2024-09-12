#include "enkiTS/src/TaskScheduler.h"
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
#include "Resources/GfxLoader.h"

// TODO: move this to a resource manager
#include <stb/stb_image.h>

namespace lne
{
void Renderer::Init(std::unique_ptr<Window>& window, std::shared_ptr<enki::TaskScheduler> taskScheduler)
{
    m_Context = window->GetGfxContext();
    m_Swapchain = window->GetSwapchain();
    m_GraphicsCommandBufferManager = std::make_unique<CommandBufferManager>(m_Context.GetPtr(), m_Swapchain->GetImageCount(), EQueueFamilyType::Graphics);
    m_TaskScheduler = taskScheduler;
    m_GfxLoader = lnnew GfxLoader();
    m_GfxLoader->Init(this, m_Context, m_TaskScheduler);
    m_TexturesToUpdate.reserve(128);
    for (uint32_t i = 0; i < m_Swapchain->GetImageCount(); i++)
    {
        InitFrameData(i);
    }
}

void Renderer::Nuke()
{
    m_Context->WaitIdle();
    m_GfxLoader->Nuke();
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
    m_GfxLoader.Reset();
}

void Renderer::BeginFrame()
{
    uint32_t imageIndex = m_Swapchain->GetCurrentFrameIndex();
    m_GraphicsCommandBufferManager->StartCommandBuffer(imageIndex);
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer(), vk::ImageLayout::eGeneral);
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();

    UpdateTextures();

    auto viewport = m_Swapchain->GetViewport();
    cmdBuffer.setScissor(0, viewport.GetScissor());
    auto vp = viewport.GetViewport();
    vp.y += vp.height;
    vp.height *= -1;
    cmdBuffer.setViewport(0, vp);

    m_FrameData[imageIndex].DescriptorAllocator->Clear();

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

void Renderer::BeginScene(const TransformComponent& cameraTransform, const CameraComponent& camera, const glm::vec3& sunDirection)
{
    uint32_t imageIndex = m_Swapchain->GetCurrentFrameIndex();
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    GlobalUniforms uniforms = {
        .ViewProj = camera.GetViewProj(),
        .View = camera.View,
        .Proj = camera.Proj,
        .CameraPosition = cameraTransform.Position,
        .SunDirection = sunDirection
    };

    m_FrameData[imageIndex].GlobalUniforms.CopyData(cmdBuffer, uniforms);
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
    return m_GfxLoader->CreateTexture(fullPath);
}

SafePtr<Texture> Renderer::CreateCubemapTexture(const std::vector<std::string>& faces)
{
    return m_GfxLoader->CreateCubemap(faces);
}

SafePtr<UniformBufferManager> Renderer::RegisterObject()
{
    SafePtr<UniformBufferManager> uboManager;
    uboManager.Reset(lnnew UniformBufferManager(m_Context, sizeof(glm::mat4)));
    return uboManager;
}

void Renderer::AddTextureToUpdate(SafePtr<class Texture> texture)
{
    std::lock_guard<std::mutex> lock(m_TexturesToUpdateMutex);
    m_TexturesToUpdate.push_back(texture);
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

void Renderer::UpdateTextures()
{
    std::lock_guard<std::mutex> lock(m_TexturesToUpdateMutex);
    if (m_TexturesToUpdate.empty())
        return;

    auto cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    for (auto& texture : m_TexturesToUpdate)
    {
        texture->TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferDstOptimal,
                m_Context->GetQueueFamilyIndex(EQueueFamilyType::Transfer), m_Context->GetQueueFamilyIndex(EQueueFamilyType::Graphics));

        if (texture->ShouldGenerateMips() == false)
        {
            texture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            continue;
        }

        texture->GenerateMipmaps(cmdBuffer);

        texture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    m_TexturesToUpdate.clear();
}
}
