#pragma once
#include "GfxEnums.h"
#include "Engine/Core/SafePtr.h"
#include "UniformBuffer.h"

namespace lne
{
struct GlobalUniforms
{
    glm::mat4 ViewProj;
    glm::mat4 View;
    glm::mat4 Proj;
};

struct FrameData {
    UniformBuffer GlobalUniforms;
    SafePtr<class DynamicDescriptorAllocator> DescriptorAllocator;
    vk::DescriptorSet DescriptorSet;
    vk::DescriptorSetLayout DescriptorSetLayout;

    FrameData(UniformBuffer&& globalUniforms, SafePtr<class DynamicDescriptorAllocator> descriptorAllocator, 
        vk::DescriptorSetLayout descriptorSetLayout)
        : GlobalUniforms(std::move(globalUniforms)),
        DescriptorAllocator(descriptorAllocator), 
        DescriptorSetLayout(descriptorSetLayout)
    {
    }
};

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void Init(std::unique_ptr<class Window>& window);
    void Nuke();

    [[nodiscard]] std::unique_ptr<class CommandBufferManager>& GetGraphicsCommandBufferManager() { return m_GraphicsCommandBufferManager; }

    void BeginFrame();
    void EndFrame();

    void BeginScene(const struct CameraComponent& camera);

    void BeginRenderPass(const class Framebuffer& framebuffer) const;
    void EndRenderPass(const class Framebuffer& framebuffer) const;

    void Draw(SafePtr<class Material> pipeline, struct Geometry& geometry, struct TransformComponent& objTransform);

    // TODO: move to a resource manager
    [[nodiscard]] SafePtr<class GfxPipeline> CreateGraphicsPipeline(const struct GraphicsPipelineDesc& createInfo);
    [[nodiscard]] SafePtr<class StorageBuffer> CreateGeometryBuffer(const void* data, size_t size);
    [[nodiscard]] SafePtr<class Texture> CreateTexture(const std::string& fullPath);
    [[nodiscard]] SafePtr<class Texture> CreateCubemapTexture(const std::vector<std::string>& faces);
    [[nodiscard]] SafePtr<class UniformBufferManager> RegisterObject();

private:
    SafePtr<class GfxContext> m_Context;
    SafePtr<class Swapchain> m_Swapchain;
    // TODO: move to a command buffer manager to the context (maybe)
    std::unique_ptr<class CommandBufferManager> m_GraphicsCommandBufferManager;
    std::vector<FrameData> m_FrameData;
private:
    void InitFrameData(uint32_t index);
};
}
