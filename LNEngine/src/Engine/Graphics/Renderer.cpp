#include "lnepch.h"
#include "Renderer.h"
#include "Engine/Core/Window.h"
#include "GfxContext.h"
#include "CommandBufferManager.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Graphics/Pipeline.h"
#include "Core/Utils/Defines.h"
#include "DynamicDescriptorAllocator.h"

namespace lne
{
void Renderer::Init(std::unique_ptr<Window>& window)
{
    m_Context = window->GetGfxContext();
    m_Swapchain = window->GetSwapchain();
    m_GraphicsCommandBufferManager = std::make_unique<CommandBufferManager>(m_Context, m_Swapchain->GetImageCount(), EQueueFamilyType::Graphics);

    for (uint32_t i = 0; i < m_Swapchain->GetImageCount(); i++)
    {
        InitFrameData(i);
    }
}

void Renderer::Shutdown()
{
    m_Context->WaitIdle();
    for (auto& frameData : m_FrameData)
    {
        frameData.GlobalUniforms.Destroy();
        frameData.DescriptorAllocator.Reset();
        m_Context->GetDevice().destroyDescriptorSetLayout(frameData.DescriptorSetLayout);
    }
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

    auto& viewport = m_Swapchain->GetViewport();

    cmdBuffer.setScissor(0, viewport.GetScissor());
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
    vk::SubmitInfo submitInfo = m_Swapchain->GetSubmitInfo(&cb, waitStages);
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

void Renderer::Draw(SafePtr<GraphicsPipeline> pipeline)
{
    const vk::CommandBuffer& cb = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0, { m_FrameData[m_Swapchain->GetCurrentFrameIndex()].DescriptorSet }, {});
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    pipeline->Bind(cmdBuffer);
    cmdBuffer.draw(3, 1, 0, 0);
}

SafePtr<GraphicsPipeline> Renderer::CreateGraphicsPipeline(const GraphicsPipelineDesc& createInfo)
{
    SafePtr<GraphicsPipeline> pipeline;
    pipeline.Reset(lnnew GraphicsPipeline(m_Context, createInfo));
    return pipeline;
}

void Renderer::InitFrameData(uint32_t index)
{
    m_FrameData.emplace_back(
            UniformBuffer(m_Context, sizeof(GlobalUniforms)),
            SafePtr(lnnew DynamicDescriptorAllocator(m_Context, { { vk::DescriptorType::eUniformBuffer, 1 } }, "GlobalDescAlloc" + std::to_string(index), 1024)),
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
