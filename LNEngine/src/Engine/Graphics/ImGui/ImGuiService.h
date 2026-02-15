#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Events/Events.h"
#include "Engine/Core/Events/WindowEvents.h"

namespace lne
{
struct ImGuiVulkanFrameRenderBuffers
{
    VkDeviceMemory      VertexBufferMemory;
    VkDeviceMemory      IndexBufferMemory;
    VkDeviceSize        VertexBufferSize;
    VkDeviceSize        IndexBufferSize;
    VkBuffer            VertexBuffer;
    VkBuffer            IndexBuffer;
};

struct DrawListCopy
{
    std::vector<ImDrawVert> Vtx{};
    std::vector<ImDrawIdx>  Idx{};
    std::vector<ImDrawCmd>  Cmds{};
};

struct DrawDataCopy
{
    ImVec2 DisplayPos{};
    ImVec2 DisplaySize{};
    ImVec2 FramebufferScale{};
    int TotalVtxCount{};
    int TotalIdxCount{};
    std::vector<DrawListCopy> Lists{};
};

class ImGuiService
{
public:
    ImGuiService();
    ~ImGuiService();

    void Init(std::unique_ptr<class Window>& window);
    void Nuke();

    void BeginFrame();
    void EndFrame();

    void CreateFontsTexture();

private:
    SafePtr<class GfxContext> m_GraphicsContext;
    SafePtr<class Swapchain> m_Swapchain;
    vk::DescriptorPool m_DescriptorPool;
    std::vector<class Framebuffer> m_Framebuffers;

    u32 m_MinImageCount{};
    u32 m_ImageCount{};

private:
    void                        InitVulkanBackend();
    void                        NukeVulkanBackend();

    void                        CreateDeviceObjects();
    void                        CreatePipeline();
    void                        CreateShaderModules();
    void                        CreateOrResizeBuffer(VkBuffer& buffer, 
                                                     VkDeviceMemory& buffer_memory, 
                                                     VkDeviceSize& buffer_size,
                                                     size_t new_size, 
                                                     VkBufferUsageFlagBits usage);

    u32                    VulkanMemoryType(VkMemoryPropertyFlags properties, 
                                                 u32 type_bits);

    void                        RenderDrawData(const DrawDataCopy& draw_data,
                                               vk::CommandBuffer cmdBuffer);

    void                        SetupRenderState(const DrawDataCopy& draw_data,
                                                 VkPipeline pipeline, 
                                                 VkCommandBuffer command_buffer, 
                                                 ImGuiVulkanFrameRenderBuffers* rb, int fb_width, int fb_height);
};
}

