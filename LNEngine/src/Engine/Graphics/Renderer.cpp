#include "lnepch.h"
#include "Renderer.h"
#include "Engine/Core/Window.h"
#include "GfxContext.h"
#include "CommandBufferManager.h"
#include "Texture.h"

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
}

void Renderer::BeginFrame()
{
    static uint32_t frameIndex = 0;
    ++frameIndex;
    uint32_t imageIndex = m_Swapchain->GetCurrentFrameIndex();
    m_GraphicsCommandBufferManager->StartCommandBuffer(imageIndex);
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer(), vk::ImageLayout::eGeneral);

    vk::ClearColorValue clearValue;
    float flash = std::abs(std::sin(frameIndex / 120.f));
    clearValue = { 0.0f, 0.0f, flash, 1.0f };
    vk::ImageSubresourceRange clearRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    m_GraphicsCommandBufferManager->GetCurrentCommandBuffer().clearColorImage(currentImage->GetImage(), vk::ImageLayout::eGeneral, clearValue, clearRange);
}

void Renderer::EndFrame()
{
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(m_GraphicsCommandBufferManager->GetCurrentCommandBuffer(), vk::ImageLayout::ePresentSrcKHR);

    const vk::CommandBuffer& cb = m_GraphicsCommandBufferManager->GetCurrentCommandBuffer();
    vk::SubmitInfo submitInfo = m_Swapchain->GetSubmitInfo(&cb, vk::PipelineStageFlagBits::eColorAttachmentOutput, false);
    m_GraphicsCommandBufferManager->Submit(submitInfo);
}

void Renderer::BeginRenderPass(Framebuffer& framebuffer)
{
}
void Renderer::EndRenderPass(Framebuffer& framebuffer)
{}
}
