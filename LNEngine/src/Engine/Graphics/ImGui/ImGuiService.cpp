#include "ImGuiService.h"
#include "Core/ApplicationBase.h"
#include "Core/Window.h"
#include "../GfxContext.h"
#include "Graphics/Resources/Texture.h"
#include "../Renderer.h"
#include "../CommandPoolManager.h"
#include "../DynamicDescriptorAllocator.h"
#include "../Framebuffer.h"
#include "Engine/Resources/GfxLoader.h"
#include "Core/Utils/Log.h"
#include <Core/Utils/Profiling.h>
#include "Core/Utils/_Defines.h"

namespace lne
{
// A LOT OF THIS CODE IS COPIED FROM THE IMGUI DIRECTLY
// I NEEDED TO DO A CUSTOM BACKEND SINCE I'M USING BINSLESS TEXTURES

// check the imgui.glsl file for the shaders
static u32 g_GlslVertSpv[] = {
    0x07230203,0x00010000,0x0008000b,0x00000035,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x000b000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x0000000f,0x00000015,
    0x0000001b,0x00000025,0x00000026,0x00030003,0x00000002,0x000001cc,0x00080004,0x455f4c47,
    0x6e5f5458,0x6e756e6f,0x726f6669,0x75715f6d,0x66696c61,0x00726569,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00030005,0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,
    0x6f6c6f43,0x00000072,0x00040006,0x00000009,0x00000001,0x00005655,0x00030005,0x0000000b,
    0x0074754f,0x00040005,0x0000000f,0x6c6f4361,0x0000726f,0x00030005,0x00000015,0x00565561,
    0x00060005,0x0000001b,0x74786554,0x49657275,0x7865646e,0x00000000,0x00060005,0x0000001c,
    0x73755075,0x6e6f4368,0x6e617473,0x00000074,0x00050006,0x0000001c,0x00000000,0x61635375,
    0x0000656c,0x00060006,0x0000001c,0x00000001,0x61725475,0x616c736e,0x00006574,0x00070006,
    0x0000001c,0x00000002,0x78655475,0x65727574,0x65646e49,0x00000078,0x00030005,0x0000001e,
    0x00006370,0x00060005,0x00000023,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,
    0x00000023,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x00000025,0x00000000,
    0x00040005,0x00000026,0x736f5061,0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000000,
    0x00040047,0x0000000f,0x0000001e,0x00000002,0x00040047,0x00000015,0x0000001e,0x00000001,
    0x00030047,0x0000001b,0x0000000e,0x00040047,0x0000001b,0x0000001e,0x00000002,0x00050048,
    0x0000001c,0x00000000,0x00000023,0x00000000,0x00050048,0x0000001c,0x00000001,0x00000023,
    0x00000008,0x00050048,0x0000001c,0x00000002,0x00000023,0x00000010,0x00030047,0x0000001c,
    0x00000002,0x00050048,0x00000023,0x00000000,0x0000000b,0x00000000,0x00030047,0x00000023,
    0x00000002,0x00040047,0x00000026,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,
    0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
    0x00000004,0x00040017,0x00000008,0x00000006,0x00000002,0x0004001e,0x00000009,0x00000007,
    0x00000008,0x00040020,0x0000000a,0x00000003,0x00000009,0x0004003b,0x0000000a,0x0000000b,
    0x00000003,0x00040015,0x0000000c,0x00000020,0x00000001,0x0004002b,0x0000000c,0x0000000d,
    0x00000000,0x00040020,0x0000000e,0x00000001,0x00000007,0x0004003b,0x0000000e,0x0000000f,
    0x00000001,0x00040020,0x00000011,0x00000003,0x00000007,0x0004002b,0x0000000c,0x00000013,
    0x00000001,0x00040020,0x00000014,0x00000001,0x00000008,0x0004003b,0x00000014,0x00000015,
    0x00000001,0x00040020,0x00000017,0x00000003,0x00000008,0x00040015,0x00000019,0x00000020,
    0x00000000,0x00040020,0x0000001a,0x00000003,0x00000019,0x0004003b,0x0000001a,0x0000001b,
    0x00000003,0x0005001e,0x0000001c,0x00000008,0x00000008,0x00000019,0x00040020,0x0000001d,
    0x00000009,0x0000001c,0x0004003b,0x0000001d,0x0000001e,0x00000009,0x0004002b,0x0000000c,
    0x0000001f,0x00000002,0x00040020,0x00000020,0x00000009,0x00000019,0x0003001e,0x00000023,
    0x00000007,0x00040020,0x00000024,0x00000003,0x00000023,0x0004003b,0x00000024,0x00000025,
    0x00000003,0x0004003b,0x00000014,0x00000026,0x00000001,0x00040020,0x00000028,0x00000009,
    0x00000008,0x0004002b,0x00000006,0x0000002f,0x00000000,0x0004002b,0x00000006,0x00000030,
    0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,
    0x0004003d,0x00000007,0x00000010,0x0000000f,0x00050041,0x00000011,0x00000012,0x0000000b,
    0x0000000d,0x0003003e,0x00000012,0x00000010,0x0004003d,0x00000008,0x00000016,0x00000015,
    0x00050041,0x00000017,0x00000018,0x0000000b,0x00000013,0x0003003e,0x00000018,0x00000016,
    0x00050041,0x00000020,0x00000021,0x0000001e,0x0000001f,0x0004003d,0x00000019,0x00000022,
    0x00000021,0x0003003e,0x0000001b,0x00000022,0x0004003d,0x00000008,0x00000027,0x00000026,
    0x00050041,0x00000028,0x00000029,0x0000001e,0x0000000d,0x0004003d,0x00000008,0x0000002a,
    0x00000029,0x00050085,0x00000008,0x0000002b,0x00000027,0x0000002a,0x00050041,0x00000028,
    0x0000002c,0x0000001e,0x00000013,0x0004003d,0x00000008,0x0000002d,0x0000002c,0x00050081,
    0x00000008,0x0000002e,0x0000002b,0x0000002d,0x00050051,0x00000006,0x00000031,0x0000002e,
    0x00000000,0x00050051,0x00000006,0x00000032,0x0000002e,0x00000001,0x00070050,0x00000007,
    0x00000033,0x00000031,0x00000032,0x0000002f,0x00000030,0x00050041,0x00000011,0x00000034,
    0x00000025,0x0000000d,0x0003003e,0x00000034,0x00000033,0x000100fd,0x00010038
};

static u32 g_GlslFragSpv[] = {
    0x07230203,0x00010000,0x0008000b,0x00000026,0x00000000,0x00020011,0x00000001,0x00020011,
    0x000014b5,0x00020011,0x000014b6,0x00020011,0x000014bb,0x0008000a,0x5f565053,0x5f545845,
    0x63736564,0x74706972,0x695f726f,0x7865646e,0x00676e69,0x0006000b,0x00000001,0x4c534c47,
    0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,0x0008000f,0x00000004,
    0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x0000001a,0x00030010,0x00000004,
    0x00000007,0x00030003,0x00000002,0x000001cc,0x00080004,0x455f4c47,0x6e5f5458,0x6e756e6f,
    0x726f6669,0x75715f6d,0x66696c61,0x00726569,0x00040005,0x00000004,0x6e69616d,0x00000000,
    0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000b,0x00000000,0x00050006,
    0x0000000b,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x0000000b,0x00000001,0x00005655,
    0x00030005,0x0000000d,0x00006e49,0x00060005,0x00000017,0x626f6c67,0x65546c61,0x72757478,
    0x00007365,0x00060005,0x0000001a,0x74786554,0x49657275,0x7865646e,0x00000000,0x00040047,
    0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,0x00000000,0x00040047,
    0x00000017,0x00000022,0x00000000,0x00040047,0x00000017,0x00000021,0x00000000,0x00030047,
    0x0000001a,0x0000000e,0x00040047,0x0000001a,0x0000001e,0x00000002,0x00030047,0x0000001c,
    0x000014b4,0x00030047,0x0000001e,0x000014b4,0x00030047,0x0000001f,0x000014b4,0x00020013,
    0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,
    0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,
    0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,0x00000002,0x0004001e,
    0x0000000b,0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,0x0000000b,0x0004003b,
    0x0000000c,0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,0x00000001,0x0004002b,
    0x0000000e,0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,0x00000007,0x00090019,
    0x00000013,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,
    0x0003001b,0x00000014,0x00000013,0x0003001d,0x00000015,0x00000014,0x00040020,0x00000016,
    0x00000000,0x00000015,0x0004003b,0x00000016,0x00000017,0x00000000,0x00040015,0x00000018,
    0x00000020,0x00000000,0x00040020,0x00000019,0x00000001,0x00000018,0x0004003b,0x00000019,
    0x0000001a,0x00000001,0x00040020,0x0000001d,0x00000000,0x00000014,0x0004002b,0x0000000e,
    0x00000020,0x00000001,0x00040020,0x00000021,0x00000001,0x0000000a,0x00050036,0x00000002,
    0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x00050041,0x00000010,0x00000011,
    0x0000000d,0x0000000f,0x0004003d,0x00000007,0x00000012,0x00000011,0x0004003d,0x00000018,
    0x0000001b,0x0000001a,0x00040053,0x00000018,0x0000001c,0x0000001b,0x00050041,0x0000001d,
    0x0000001e,0x00000017,0x0000001c,0x0004003d,0x00000014,0x0000001f,0x0000001e,0x00050041,
    0x00000021,0x00000022,0x0000000d,0x00000020,0x0004003d,0x0000000a,0x00000023,0x00000022,
    0x00050057,0x00000007,0x00000024,0x0000001f,0x00000023,0x00050085,0x00000007,0x00000025,
    0x00000012,0x00000024,0x0003003e,0x00000009,0x00000025,0x000100fd,0x00010038
};

// Reusable buffers used for rendering 1 current in-flight frame, for ImGui_ImplVulkan_RenderDrawData()
// [Please zero-clear before use!]
// TODO: Create a more customized version of this struct using my own buffers

#pragma region ImGui backend helpers
//////////////////////////////////////////////////////////////////////////
// ImGui Vulkan backend data /////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Each viewport will hold 1 ImGui_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct ImGuiVulkanWindowRenderBuffers
{
    u32            Index;
    u32            Count;
    ImGuiVulkanFrameRenderBuffers* FrameRenderBuffers;
};

struct ImGuiVulkanViewportData
{
    ImGui_ImplVulkanH_Window                Window;                 // Used by secondary viewports only
    ImGuiVulkanWindowRenderBuffers          RenderBuffers;          // Used by all viewports
    bool                                    WindowOwned;
    bool                                    SwapChainNeedRebuild;   // Flag when viewport swapchain resized in the middle of processing a frame

    ImGuiVulkanViewportData() { WindowOwned = SwapChainNeedRebuild = false; memset(&RenderBuffers, 0, sizeof(RenderBuffers)); }
    ~ImGuiVulkanViewportData() {}
};

struct ImGuiVulkanData
{
    vk::DeviceSize                BufferMemoryAlignment;
    vk::PipelineCreateFlags       PipelineCreateFlags;
    vk::DescriptorSetLayout       DescriptorSetLayout;
    vk::PipelineLayout            PipelineLayout;
    vk::Pipeline                  Pipeline;               // pipeline for main render pass (created by app)
    vk::Pipeline                  PipelineForViewports;   // pipeline for secondary viewports (created by backend)
    vk::ShaderModule              ShaderModuleVert;
    vk::ShaderModule              ShaderModuleFrag;

    // Font data
    vk::Sampler                   FontSampler;
    SafePtr<Texture>              FontTexture;

    // Render buffers for main window
    ImGuiVulkanWindowRenderBuffers MainWindowRenderBuffers;

    ImGuiVulkanData()
    {
        memset((void*)this, 0, sizeof(*this));
        BufferMemoryAlignment = 256;
    }
};

static ImGuiVulkanData* ImGuiGetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGuiVulkanData*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

#pragma endregion

DrawDataCopy CloneImGuiDrawData(const ImDrawData* src)
{
    DrawDataCopy out;
    if (!src) return out;

    out.DisplayPos = src->DisplayPos;
    out.DisplaySize = src->DisplaySize;
    out.FramebufferScale = src->FramebufferScale;

    out.TotalIdxCount = src->TotalIdxCount;   // copy from ImGui
    out.TotalVtxCount = src->TotalVtxCount;   // copy from ImGui

    out.Lists.resize(src->CmdListsCount);

    for (int i = 0; i < src->CmdListsCount; ++i)
    {
        const ImDrawList* s = src->CmdLists[i];
        auto& d = out.Lists[i];

        // (Optional) Reserve to avoid re-allocs
        d.Vtx.reserve(s->VtxBuffer.Size);
        d.Idx.reserve(s->IdxBuffer.Size);
        d.Cmds.reserve(s->CmdBuffer.Size);

        d.Vtx.assign(s->VtxBuffer.Data, s->VtxBuffer.Data + s->VtxBuffer.Size);
        d.Idx.assign(s->IdxBuffer.Data, s->IdxBuffer.Data + s->IdxBuffer.Size);
        d.Cmds.assign(s->CmdBuffer.Data, s->CmdBuffer.Data + s->CmdBuffer.Size);
        // NOTE: If you use UserCallback, you’re copying the pointer; ensure callback+userdata stay valid/thread-safe.
    }

#ifdef LNE_DEBUG
    // Small sanity check (useful during bring-up)
    int sumIdx = 0, sumVtx = 0;
    for (const auto& l : out.Lists)
    {
        sumIdx += static_cast<int>(l.Idx.size());
        sumVtx += static_cast<int>(l.Vtx.size());
    }
    // If these trigger, something mutated src between counting and copying (shouldn't happen).
    LNE_ASSERT(sumIdx == out.TotalIdxCount, "ImGui DrawData cloning error: Mismatched index count");
    LNE_ASSERT(sumVtx == out.TotalVtxCount, "ImGui DrawData cloning error: Mismatched vertex count");
#endif

    return out;
}

static void ImGuiNukeFrameRenderBuffers(VkDevice device, ImGuiVulkanFrameRenderBuffers* rb)
{
    if (rb->VertexBuffer) { vkDestroyBuffer(device, rb->VertexBuffer, nullptr); rb->VertexBuffer = VK_NULL_HANDLE; }
    if (rb->VertexBufferMemory) { vkFreeMemory(device, rb->VertexBufferMemory, nullptr); rb->VertexBufferMemory = VK_NULL_HANDLE; }
    if (rb->IndexBuffer) { vkDestroyBuffer(device, rb->IndexBuffer, nullptr); rb->IndexBuffer = VK_NULL_HANDLE; }
    if (rb->IndexBufferMemory) { vkFreeMemory(device, rb->IndexBufferMemory, nullptr); rb->IndexBufferMemory = VK_NULL_HANDLE; }
    rb->VertexBufferSize = 0;
    rb->IndexBufferSize = 0;
}

void ImGuiNukeWindowRenderBuffers(VkDevice device, ImGuiVulkanWindowRenderBuffers* buffers)
{
    for (u32 n = 0; n < buffers->Count; n++)
        ImGuiNukeFrameRenderBuffers(device, &buffers->FrameRenderBuffers[n]);
    IM_FREE(buffers->FrameRenderBuffers);
    buffers->FrameRenderBuffers = nullptr;
    buffers->Index = 0;
    buffers->Count = 0;
}

static inline VkDeviceSize AlignBufferSize(VkDeviceSize size, VkDeviceSize alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

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

    vk::Format depthFormat = vk::Format::eD32Sfloat;
    for (auto& framebuffer : m_Framebuffers)
    {
        framebuffer.ChangeColorAttachmentsOps(vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
        depthFormat = framebuffer.HasDepth() ? framebuffer.GetDepthAttachment().Texture->GetFormat() : vk::Format::eUndefined;
    }

    vk::PipelineRenderingCreateInfo renderingInfo = vk::PipelineRenderingCreateInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &m_Swapchain->GetSurfaceFormat().format;
    renderingInfo.depthAttachmentFormat = depthFormat;

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

    m_MinImageCount = 2;
    m_ImageCount = m_Swapchain->GetImageCount();

    window->AddSwapchainRecreateCallback(this, [this]()
        {
            m_Framebuffers = m_Swapchain->GetFramebuffers();
            for (auto& framebuffer : m_Framebuffers)
            {
                framebuffer.ChangeColorAttachmentsOps(vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
            }
        }
    );

    //ImGui_ImplVulkan_Init(&initInfo);
    InitVulkanBackend();
}

void ImGuiService::Nuke()
{
    m_GraphicsContext->WaitIdle();
    //ImGui_ImplVulkan_Shutdown();
    NukeVulkanBackend();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_GraphicsContext->GetDevice().destroyDescriptorPool(m_DescriptorPool);
    m_Framebuffers.clear();
    m_GraphicsContext.Reset();
    m_Swapchain.Reset();
}

void ImGuiService::BeginFrame()
{
    //ImGui_ImplVulkan_NewFrame();
    ImGuiVulkanData* bd = ImGuiGetBackendData();
    LNE_ASSERT(bd, "ImGui backend data is null");
    if (!bd->FontTexture)
        CreateFontsTexture();

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiService::EndFrame()
{
    ImGui::ShowDemoWindow();
    {
        LNE_PROFILE_SCOPE("ImGui Render");
        ImGui::Render();
    }

    {
        LNE_PROFILE_SCOPE("ImGui Clone Data & Sumbit");
        DrawDataCopy ddCopy = CloneImGuiDrawData(ImGui::GetDrawData());

        auto& renderer = ApplicationBase::GetRenderer();

        auto imGuiRenderCommand = [this, ddCopy = std::move(ddCopy)]()
            {
                LNE_PROFILE_SCOPE("ImGui Render");
                u32 imageIndex = m_Swapchain->GetCurrentFrameIndex();
                auto& renderer = ApplicationBase::GetRenderer();
                CommandBuffer* cmdBuffer = m_GraphicsContext->GetPrimaryCommandBuffer();

                cmdBuffer->PushLabel("ImGui");
                m_Framebuffers[imageIndex].Bind(cmdBuffer);

                RenderDrawData(ddCopy, cmdBuffer->GetVkCommandBuffer());

                m_Framebuffers[imageIndex].Unbind(cmdBuffer);
                cmdBuffer->PopLabel();
            };

        if (renderer.IsAsync())
            renderer.AddRenderTask(imGuiRenderCommand);
        else
            imGuiRenderCommand;
    }
}

void ImGuiService::CreateFontsTexture()
{
    ImGuiVulkanData* bd = ImGuiGetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    if (bd->FontTexture != nullptr)
        bd->FontTexture = nullptr;

    u8* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    SafePtr<Texture> texture = Texture::CreateColorTexture2D(
        m_GraphicsContext,
        width, height, vk::Format::eR8G8B8A8Unorm,
        TextureUsageType::eSampled,
        false,
        "ImGui Font Texture"
    );

    UploadRequest gpuRequest = {};
    gpuRequest.Type = ResourceTypes::eTexture;
    gpuRequest.Resource = texture;
    gpuRequest.Data = pixels;
    gpuRequest.Size = width * height * 4;
    gpuRequest.ShouldFreeData = false;

    ApplicationBase::GetRenderer().GetGfxLoader()->Upload(gpuRequest);
    bd->FontTexture = texture;
    io.Fonts->SetTexID((ImTextureID)(u64)texture->GetBindlessTextureHandle());
}

void ImGuiService::InitVulkanBackend()
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGuiVulkanData* bd = IM_NEW(ImGuiVulkanData)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_vulkan";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    CreateDeviceObjects();

    // Our render function expect RendererUserData to be storing the window render buffer we need (for the main viewport we won't use ->Window)
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->RendererUserData = IM_NEW(ImGuiVulkanViewportData)();

    //if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    //    ImGui_ImplVulkan_InitPlatformInterface();
}

void ImGuiService::NukeVulkanBackend()
{
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    if (ImGuiVulkanViewportData* vd = (ImGuiVulkanViewportData*)main_viewport->RendererUserData)
    {
        ImGuiNukeWindowRenderBuffers(m_GraphicsContext->GetDevice(), &vd->RenderBuffers);
        IM_DELETE(vd);
        main_viewport->RendererUserData = nullptr;
    }

    ImGuiVulkanData* bd = ImGuiGetBackendData();
    if (!bd)
        return;
    ImGuiIO& io = ImGui::GetIO();

    if (bd->FontTexture)
        bd->FontTexture = nullptr;
    if (bd->FontSampler)
    {
        m_GraphicsContext->GetDevice().destroySampler(bd->FontSampler);
        bd->FontSampler = nullptr;
    }
    if (bd->ShaderModuleVert)
    {
        m_GraphicsContext->GetDevice().destroyShaderModule(bd->ShaderModuleVert);
        bd->ShaderModuleVert = nullptr;
    }
    if (bd->ShaderModuleFrag)
    {
        m_GraphicsContext->GetDevice().destroyShaderModule(bd->ShaderModuleFrag);
        bd->ShaderModuleFrag = nullptr;
    }
    if (bd->DescriptorSetLayout)
    {
        m_GraphicsContext->GetDevice().destroyDescriptorSetLayout(bd->DescriptorSetLayout);
        bd->DescriptorSetLayout = nullptr;
    }
    if (bd->PipelineLayout)
    {
        m_GraphicsContext->GetDevice().destroyPipelineLayout(bd->PipelineLayout);
        bd->PipelineLayout = nullptr;
    }
    if (bd->Pipeline)
    {
        m_GraphicsContext->GetDevice().destroyPipeline(bd->Pipeline);
        bd->Pipeline = nullptr;
    }
    if (bd->PipelineForViewports)
    {
        m_GraphicsContext->GetDevice().destroyPipeline(bd->PipelineForViewports);
        bd->PipelineForViewports = nullptr;
    }

    ImGui::DestroyPlatformWindows();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasViewports);
    IM_DELETE(bd);
}

void ImGuiService::CreateDeviceObjects()
{
    ImGuiVulkanData* bd = ImGuiGetBackendData();

    // create sampler for font textures
    if (!bd->FontSampler)
    {
        bd->FontSampler = m_GraphicsContext->CreateSampler(
            vk::Filter::eLinear, vk::Filter::eLinear,
            vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat,
            1.0f,
            false, vk::CompareOp::eAlways,
            0.0f, 16.0f, 0.0f,
            vk::BorderColor::eFloatOpaqueWhite,
            vk::SamplerReductionMode::eWeightedAverage,
            "ImGui Sampler"
        );
    }

    // create descriptor set layout
    if (!bd->DescriptorSetLayout)
    {
        u32 bindlessPoolSize = 2048;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindlessLayoutBindings{
            vk::DescriptorSetLayoutBinding{
                0,
                vk::DescriptorType::eCombinedImageSampler,
                bindlessPoolSize,
                vk::ShaderStageFlagBits::eAll
            },
            vk::DescriptorSetLayoutBinding{
                1,
                vk::DescriptorType::eStorageImage,
                bindlessPoolSize,
                vk::ShaderStageFlagBits::eAll
            }
        };

        std::array<vk::DescriptorBindingFlags, 2> bindlessBindingFlags{
            vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound,
            vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound
        };

        vk::DescriptorSetLayoutBindingFlagsCreateInfo bindlessLayoutBindingFlags{
            bindlessBindingFlags
        };

        vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> bindlessLayoutChain{
            vk::DescriptorSetLayoutCreateInfo{
                vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
                bindlessLayoutBindings
            },
            bindlessLayoutBindingFlags
        };

        bd->DescriptorSetLayout = m_GraphicsContext->GetDevice().createDescriptorSetLayout(bindlessLayoutChain.get<vk::DescriptorSetLayoutCreateInfo>());

        m_GraphicsContext->SetVkObjectName(bd->DescriptorSetLayout, "ImGui DescriptorSetLayout");
    }

    if (!bd->PipelineLayout)
    {
        // create pipeline layout
        vk::PushConstantRange pushConstantRange[1] = {};
        pushConstantRange[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
        pushConstantRange[0].offset = 0;
        pushConstantRange[0].size = sizeof(float) * 4 + sizeof(u32); // 2 vec2 + 1 uint

        vk::DescriptorSetLayout setLayouts[1] = { bd->DescriptorSetLayout };
        vk::PipelineLayoutCreateInfo pipelineLayoutCI = {};
        pipelineLayoutCI.setLayoutCount = 1;
        pipelineLayoutCI.pSetLayouts = setLayouts;
        pipelineLayoutCI.pushConstantRangeCount = 1;
        pipelineLayoutCI.pPushConstantRanges = pushConstantRange;
        bd->PipelineLayout = m_GraphicsContext->GetDevice().createPipelineLayout(pipelineLayoutCI);
    }

    if (!bd->Pipeline)
        CreatePipeline();
}

void ImGuiService::CreatePipeline()
{
    ImGuiVulkanData* bd = ImGuiGetBackendData();
    CreateShaderModules();

    vk::PipelineShaderStageCreateInfo stage[2] = {};
    stage[0].stage = vk::ShaderStageFlagBits::eVertex;
    stage[0].module = bd->ShaderModuleVert;
    stage[0].pName = "main";
    stage[1].stage = vk::ShaderStageFlagBits::eFragment;
    stage[1].module = bd->ShaderModuleFrag;
    stage[1].pName = "main";

    vk::VertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(ImDrawVert);
    binding_desc[0].inputRate = vk::VertexInputRate::eVertex;

    vk::VertexInputAttributeDescription attribute_desc[3] = {};
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = vk::Format::eR32G32Sfloat;
    attribute_desc[0].offset = offsetof(ImDrawVert, pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = vk::Format::eR32G32Sfloat;
    attribute_desc[1].offset = offsetof(ImDrawVert, uv);
    attribute_desc[2].location = 2;
    attribute_desc[2].binding = binding_desc[0].binding;
    attribute_desc[2].format = vk::Format::eR8G8B8A8Unorm;
    attribute_desc[2].offset = offsetof(ImDrawVert, col);

    vk::PipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = 3;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    vk::PipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.polygonMode = vk::PolygonMode::eFill;
    raster_info.cullMode = vk::CullModeFlagBits::eNone;
    raster_info.frontFace = vk::FrontFace::eCounterClockwise;
    raster_info.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    color_attachment[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_attachment[0].colorBlendOp = vk::BlendOp::eAdd;
    color_attachment[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
    color_attachment[0].dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_attachment[0].alphaBlendOp = vk::BlendOp::eAdd;
    color_attachment[0].colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineDepthStencilStateCreateInfo depth_info = {};

    vk::PipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    vk::DynamicState dynamic_states[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    vk::GraphicsPipelineCreateInfo info = {};
    info.flags = bd->PipelineCreateFlags;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &ia_info;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = bd->PipelineLayout;

    m_Framebuffers = m_Swapchain->GetFramebuffers();

    vk::Format depthFormat = vk::Format::eD32Sfloat;
    for (auto& framebuffer : m_Framebuffers)
    {
        framebuffer.ChangeColorAttachmentsOps(vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
        depthFormat = framebuffer.HasDepth() ? framebuffer.GetDepthAttachment().Texture->GetFormat() : vk::Format::eUndefined;
    }

    vk::PipelineRenderingCreateInfo renderingInfo = vk::PipelineRenderingCreateInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &m_Swapchain->GetSurfaceFormat().format;
    renderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain{
        info,
        renderingInfo
    };

    auto result = m_GraphicsContext->GetDevice().createGraphicsPipeline(nullptr, 
        chain.get<vk::GraphicsPipelineCreateInfo>(), nullptr);

    if (result.result != vk::Result::eSuccess)
    {
        LNE_ERROR("Failed to create graphics pipeline: {}", vk::to_string(result.result));
        return;
    }
    bd->Pipeline = result.value;
    m_GraphicsContext->SetVkObjectName(bd->Pipeline, "GraphicsPipeline: ImGUI");
}

void ImGuiService::CreateShaderModules()
{
    // Create the shader modules
    ImGuiVulkanData* bd = ImGuiGetBackendData();

    if (bd->ShaderModuleVert == VK_NULL_HANDLE)
    {
        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vert_info.codeSize = sizeof(g_GlslVertSpv);
        vert_info.pCode = (u32*)g_GlslVertSpv;
        bd->ShaderModuleVert = m_GraphicsContext->GetDevice().createShaderModule(vert_info, nullptr);
    }

    if (bd->ShaderModuleFrag == VK_NULL_HANDLE)
    {
        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_info.codeSize = sizeof(g_GlslFragSpv);
        frag_info.pCode = (u32*)g_GlslFragSpv;
        bd->ShaderModuleFrag = m_GraphicsContext->GetDevice().createShaderModule(frag_info, nullptr);
    }
}

void ImGuiService::CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& buffer_size, size_t new_size, VkBufferUsageFlagBits usage)
{
    ImGuiVulkanData* bd = ImGuiGetBackendData();
    VkResult err;
    vk::Device device = m_GraphicsContext->GetDevice();
    if (buffer != VK_NULL_HANDLE)
        device.destroyBuffer(buffer, nullptr);
    if (buffer_memory != VK_NULL_HANDLE)
        device.freeMemory(buffer_memory, nullptr);

    VkDeviceSize buffer_size_aligned = AlignBufferSize(std::max((size_t)1024*1024, new_size), bd->BufferMemoryAlignment);
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(device, &buffer_info, nullptr, &buffer);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buffer, &req);
    bd->BufferMemoryAlignment = (bd->BufferMemoryAlignment > req.alignment) ? bd->BufferMemoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = VulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory);

    err = vkBindBufferMemory(device, buffer, buffer_memory, 0);
    buffer_size = buffer_size_aligned;
}

u32 ImGuiService::VulkanMemoryType(VkMemoryPropertyFlags properties, u32 type_bits)
{
    ImGuiVulkanData* bd = ImGuiGetBackendData();
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(m_GraphicsContext->GetPhysicalDevice(), &prop);
    for (u32 i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}

void ImGuiService::RenderDrawData(const DrawDataCopy& draw_data, vk::CommandBuffer cmdBuffer)
{
    int fb_width = (int)(draw_data.DisplaySize.x * draw_data.FramebufferScale.x);
    int fb_height = (int)(draw_data.DisplaySize.y * draw_data.FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    ImGuiVulkanData* bd = ImGuiGetBackendData();

    if (!bd->Pipeline)
    {
        LNE_ERROR("ImGui pipeline is null");
        return;
    }

    ImGuiVulkanViewportData* vd = (ImGuiVulkanViewportData*)ImGui::GetMainViewport()->RendererUserData;
    if (!vd)
    {
        LNE_ERROR("ImGui viewport data is null");
        return;
    }

    ImGuiVulkanWindowRenderBuffers* wrb = &vd->RenderBuffers;
    if (wrb->FrameRenderBuffers == nullptr)
    {
        wrb->Index = 0;
        wrb->Count = m_ImageCount;
        wrb->FrameRenderBuffers = (ImGuiVulkanFrameRenderBuffers*)IM_ALLOC(sizeof(ImGuiVulkanFrameRenderBuffers) * wrb->Count);
        memset(wrb->FrameRenderBuffers, 0, sizeof(ImGuiVulkanFrameRenderBuffers) * wrb->Count);
    }
    LNE_ASSERT(wrb->Count == m_ImageCount, "FrameRenderBuffers count mismatch");
    wrb->Index = (wrb->Index + 1) % wrb->Count;
    ImGuiVulkanFrameRenderBuffers* rb = &wrb->FrameRenderBuffers[wrb->Index];

    vk::Device device = m_GraphicsContext->GetDevice();

    if (draw_data.TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertex_size = AlignBufferSize(draw_data.TotalVtxCount * sizeof(ImDrawVert), bd->BufferMemoryAlignment);
        size_t index_size = AlignBufferSize(draw_data.TotalIdxCount * sizeof(ImDrawIdx), bd->BufferMemoryAlignment);
        if (rb->VertexBuffer == VK_NULL_HANDLE || rb->VertexBufferSize < vertex_size)
            CreateOrResizeBuffer(rb->VertexBuffer, rb->VertexBufferMemory, rb->VertexBufferSize, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        if (rb->IndexBuffer == VK_NULL_HANDLE || rb->IndexBufferSize < index_size)
            CreateOrResizeBuffer(rb->IndexBuffer, rb->IndexBufferMemory, rb->IndexBufferSize, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = nullptr;
        ImDrawIdx* idx_dst = nullptr;
        VkResult err = vkMapMemory(device, rb->VertexBufferMemory, 0, vertex_size, 0, (void**)&vtx_dst);
        err = vkMapMemory(device, rb->IndexBufferMemory, 0, index_size, 0, (void**)&idx_dst);
        for (int n = 0; n < draw_data.Lists.size(); n++)
        {
            const DrawListCopy& cmd_list = draw_data.Lists[n];
            memcpy(vtx_dst, cmd_list.Vtx.data(), cmd_list.Vtx.size() * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list.Idx.data(), cmd_list.Idx.size() * sizeof(ImDrawIdx));
            vtx_dst += cmd_list.Vtx.size();
            idx_dst += cmd_list.Idx.size();
        }
        VkMappedMemoryRange range[2] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = rb->VertexBufferMemory;
        range[0].size = VK_WHOLE_SIZE;
        range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[1].memory = rb->IndexBufferMemory;
        range[1].size = VK_WHOLE_SIZE;
        err = vkFlushMappedMemoryRanges(device, 2, range);

        vkUnmapMemory(device, rb->VertexBufferMemory);
        vkUnmapMemory(device, rb->IndexBufferMemory);
    }

    // Setup render state
    SetupRenderState(draw_data, bd->Pipeline, cmdBuffer, rb, fb_width, fb_height);

    ImVec2 clip_off = draw_data.DisplayPos;
    ImVec2 clip_scale = draw_data.FramebufferScale;

    int global_vtx_offset = 0;
    int global_idx_offset = 0;

    for (int n = 0; n < draw_data.Lists.size(); n++)
    {
        const DrawListCopy& cmd_list = draw_data.Lists[n];
        for (int cmd_i = 0; cmd_i < cmd_list.Cmds.size(); cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list.Cmds[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    SetupRenderState(draw_data, bd->Pipeline, cmdBuffer, rb, fb_width, fb_height);
                else
                    LNE_ERROR("Custom user callbacks not supported in this ImGui implementation");
                //    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x = (s32)(clip_min.x);
                scissor.offset.y = (s32)(clip_min.y);
                scissor.extent.width = (u32)(clip_max.x - clip_min.x);
                scissor.extent.height = (u32)(clip_max.y - clip_min.y);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                // Bind DescriptorSet with font or user texture
                VkDescriptorSet desc_set[1] = { (VkDescriptorSet)m_GraphicsContext->GetBindlessDescriptorSet() };
                u32 texId = pcmd->TextureId ? (u32)(intptr_t)pcmd->TextureId : 0;
                vkCmdPushConstants(cmdBuffer, bd->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 4, sizeof(u32), &texId);
                if (sizeof(ImTextureID) < sizeof(ImU64))
                {
                    // We don't support texture switches if ImTextureID hasn't been redefined to be 64-bit. Do a flaky check that other textures haven't been used.
                    LNE_ASSERT(pcmd->TextureId == (ImTextureID)(u64)bd->FontTexture->GetBindlessTextureHandle(), "");
                    texId = bd->FontTexture->GetBindlessTextureHandle();
                    vkCmdPushConstants(cmdBuffer, bd->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 4, sizeof(u32), &texId);
                }
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bd->PipelineLayout, 0, 1, desc_set, 0, nullptr);

                // Draw
                vkCmdDrawIndexed(cmdBuffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += (int)cmd_list.Idx.size();
        global_vtx_offset += (int)cmd_list.Vtx.size();
    }
    VkRect2D scissor = { { 0, 0 }, { (u32)fb_width, (u32)fb_height } };
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
}

void ImGuiService::SetupRenderState(const DrawDataCopy& draw_data, VkPipeline pipeline, VkCommandBuffer cmdBuffer, ImGuiVulkanFrameRenderBuffers* rb, int fbWidth, int fbHeight)
{
    ImGuiVulkanData* bd = ImGuiGetBackendData();

    // bind pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind Vertex And Index Buffer:
    if (draw_data.TotalVtxCount > 0)
    {
        VkBuffer vertex_buffers[1] = { rb->VertexBuffer };
        VkDeviceSize vertex_offset[1] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertex_buffers, vertex_offset);
        vkCmdBindIndexBuffer(cmdBuffer, rb->IndexBuffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    // Setup viewport:
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)fbWidth;
    viewport.height = (float)fbHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    float scale[2];
    scale[0] = 2.0f / draw_data.DisplaySize.x;
    scale[1] = 2.0f / draw_data.DisplaySize.y;
    float translate[2];
    translate[0] = -1.0f - draw_data.DisplayPos.x * scale[0];
    translate[1] = -1.0f - draw_data.DisplayPos.y * scale[1];
    vkCmdPushConstants(cmdBuffer, bd->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
    vkCmdPushConstants(cmdBuffer, bd->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
}
}
