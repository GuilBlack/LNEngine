#include "ImGuiService.h"
#include "Core/ApplicationBase.h"
#include "Core/Window.h"
#include "../GfxContext.h"
#include "../Texture.h"
#include "../Renderer.h"
#include "../CommandBufferManager.h"
#include "../DynamicDescriptorAllocator.h"
#include "../Framebuffer.h"

namespace lne
{
ImGuiService::ImGuiService()
{}

ImGuiService::~ImGuiService()
{}

void ImGuiService::Init(std::unique_ptr<Window>& window)
{
    m_GraphicsContext = window->GetGfxContext();

    m_Swapchain = window->GetSwapchain();

    std::vector<vk::DescriptorPoolSize> pool_sizes = { 
        { vk::DescriptorType::eSampler, 512 },
        { vk::DescriptorType::eCombinedImageSampler, 512 },
        { vk::DescriptorType::eSampledImage, 512 },
        { vk::DescriptorType::eStorageImage, 512 },
        { vk::DescriptorType::eUniformTexelBuffer, 512 },
        { vk::DescriptorType::eStorageTexelBuffer, 512 },
        { vk::DescriptorType::eUniformBuffer, 512 },
        { vk::DescriptorType::eStorageBuffer, 512 },
        { vk::DescriptorType::eUniformBufferDynamic, 512 },
        { vk::DescriptorType::eStorageBufferDynamic, 512 },
        { vk::DescriptorType::eInputAttachment, 512 } 
    };
    vk::DescriptorPoolCreateInfo poolCI = {};
    poolCI.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolCI.maxSets = 5'632;
    poolCI.setPoolSizes(pool_sizes);
    m_DescriptorPool = m_GraphicsContext->GetDevice().createDescriptorPool(poolCI);
    m_GraphicsContext->SetVkObjectName(m_DescriptorPool, "ImGui DescriptorPool");

    m_Framebuffers = m_Swapchain->GetFramebuffers();

    for (auto& framebuffer : m_Framebuffers)
    {
        framebuffer.ChangeColorAttachmentsOps(vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
    }

    vk::PipelineRenderingCreateInfo renderingInfo = vk::PipelineRenderingCreateInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &m_Swapchain->GetSurfaceFormat().format;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForVulkan(window->GetHandle(), true);

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = GfxContext::VulkanInstance(),
        .PhysicalDevice = m_GraphicsContext->GetPhysicalDevice(),
        .Device = m_GraphicsContext->GetDevice(),
        .QueueFamily = m_GraphicsContext->GetQueueFamilyIndex(EQueueFamilyType::Graphics),
        .Queue = m_GraphicsContext->GetQueue(EQueueFamilyType::Graphics),
        .DescriptorPool = m_DescriptorPool,
        .MinImageCount = 2,
        .ImageCount = m_Swapchain->GetImageCount(),
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = renderingInfo
    };

    window->AddSwapchainRecreateCallback(this, [this]()
        {
            m_Framebuffers = m_Swapchain->GetFramebuffers();
            for (auto& framebuffer : m_Framebuffers)
            {
                framebuffer.ChangeColorAttachmentsOps(vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
            }
        }
    );

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiService::Nuke()
{
    m_GraphicsContext->WaitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_GraphicsContext->GetDevice().destroyDescriptorPool(m_DescriptorPool);
    m_Framebuffers.clear();
    m_GraphicsContext.Reset();
    m_Swapchain.Reset();
}

void ImGuiService::BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiService::EndFrame()
{
    ImGui::ShowDemoWindow();
    ImGui::Render();

    uint32_t imageIndex = m_Swapchain->GetCurrentFrameIndex();

    auto cmdBuffer = ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer();

    m_Framebuffers[imageIndex].Bind(cmdBuffer);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);

    m_Framebuffers[imageIndex].Unbind(cmdBuffer);
}
}
