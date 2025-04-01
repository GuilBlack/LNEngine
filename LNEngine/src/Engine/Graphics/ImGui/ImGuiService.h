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

    uint32_t m_MinImageCount{};
    uint32_t m_ImageCount{};

private:
    void InitVulkanBackend();
    void NukeVulkanBackend();
    void CreateDeviceObjects(); // copy paste from ImGUI but should use my own objects
    void CreatePipeline();      // copy paste from ImGUI but should use my own objects
    void CreateShaderModules(); // copy paste from ImGUI but should use my own objects
    void CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& buffer_size, size_t new_size, VkBufferUsageFlagBits usage);
    uint32_t VulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
    void RenderDrawData(ImDrawData* draw_data, vk::CommandBuffer cmdBuffer);
    void SetupRenderState(ImDrawData* draw_data, VkPipeline pipeline, VkCommandBuffer command_buffer, ImGuiVulkanFrameRenderBuffers* rb, int fb_width, int fb_height);
};
}

