#include "lnepch.h"
#include "Renderer.h"
#include "Engine/Core/Window.h"
#include "GfxContext.h"
#include "CommandBufferManager.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Graphics/Pipeline.h"

namespace lne
{
void Renderer::Init(std::unique_ptr<Window>& window)
{
    m_Context = window->GetGfxContext();
    m_Swapchain = window->GetSwapchain();
    m_GraphicsCommandBufferManager = std::make_unique<CommandBufferManager>(m_Context, m_Swapchain->GetImageCount(), EQueueFamilyType::Graphics);
}

void Renderer::Shutdown()
{
    m_Context->WaitIdle();
    m_GraphicsCommandBufferManager.reset();
    m_Context.reset();
    m_Swapchain.reset();
}

void Renderer::BeginFrame()
{
    uint32_t imageIndex = m_Swapchain->GetCurrentFrameIndex();
    m_GraphicsCommandBufferManager->StartCommandBuffer(imageIndex);
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer(), vk::ImageLayout::eGeneral);
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();

    cmdBuffer.setScissor(0, m_Swapchain->GetViewport().GetScissor());
    cmdBuffer.setViewport(0, m_Swapchain->GetViewport().GetViewport());
}

void Renderer::EndFrame()
{
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer(), vk::ImageLayout::ePresentSrcKHR);

    const vk::CommandBuffer& cb = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
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

void Renderer::Draw(std::shared_ptr<GraphicsPipeline> pipeline)
{
    auto& cmdBuffer = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    pipeline->Bind(cmdBuffer);
    cmdBuffer.draw(3, 1, 0, 0);
}

std::shared_ptr<GraphicsPipeline> Renderer::CreateGraphicsPipeline(const GraphicsPipelineDesc& createInfo)
{
    std::shared_ptr<GraphicsPipeline> pipeline;
    pipeline.reset(new GraphicsPipeline(m_Context, createInfo));
    return pipeline;
}
}
