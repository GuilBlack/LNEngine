#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Resources/GfxLoader.h"
#include "UniformBuffer.h"
#include "DynamicDescriptorAllocator.h"
#include "Mesh.h"
#include "GfxContext.h"

namespace enki
{
class TaskScheduler;
}

namespace lne
{
struct GlobalUniforms
{
    glm::mat4 ViewProj;
    glm::mat4 View;
    glm::mat4 Proj;
    glm::vec3 CameraPosition;
    glm::vec3 SunDirection;
    float     AmbientLight;
};

// TODO: move this in the World renderer
struct FrameData {
    UniformBuffer GlobalUniforms;
    SafePtr<class DynamicDescriptorAllocator> DescriptorAllocator;
    vk::DescriptorPool GlobalDescriptorPool;
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

    void Init(std::unique_ptr<class Window>& window, std::shared_ptr<enki::TaskScheduler> taskScheduler);
    void Nuke();
    void InitResources();

    [[nodiscard]] uint32_t GetCurrentFrameIndex() const { return m_Context->GetCurrentFrameIndex(); }
    [[nodiscard]] SafePtr<class GfxContext> GetGfxContext() const { return m_Context; }
    [[nodiscard]] SafePtr<class GfxLoader> GetGfxLoader() const { return m_GfxLoader; }
    [[nodiscard]] vk::DescriptorSet AllocateFrameDescSet(vk::DescriptorSetLayout layout);

    void PushLabel(vk::CommandBuffer cmdBuffer, std::string_view label) const;

    void PopLabel(vk::CommandBuffer cmdBuffer) const;

    void BeginFrame();
    void EndFrame();
    void PostFrame();

    void BeginScene(const struct TransformComponent& cameraTransform, const struct CameraComponent& camera, const glm::vec3& sunDirection, float ambientLight);

    void BeginRenderPass(const class Framebuffer& framebuffer) const;
    void EndRenderPass(const class Framebuffer& framebuffer) const;

    void Draw(SafePtr<class Material> pipeline, struct Geometry& geometry, struct TransformComponent& objTransform);
    void Draw(SafePtr<class StaticMesh> mesh, struct TransformComponent& objTransform);
    void Draw(vk::CommandBuffer cmdBuffer, const SafePtr<lne::StaticMesh>& mesh, const SafePtr<lne::StorageBuffer>& transformBuffer, uint32_t offset, uint32_t subMeshIndex, uint32_t instanceCount);
    void Draw(vk::CommandBuffer cmdBuffer, const SafePtr<lne::StaticMesh>& mesh, const SafePtr<lne::StorageBuffer>& transformBuffer, SafePtr<Material> overrideMaterial, uint32_t offset, uint32_t subMeshIndex, uint32_t instanceCount);

    void DrawFullscreenQuad(vk::CommandBuffer cmdBuffer, const SafePtr<class Material>& material);

    void Dispatch(SafePtr<class ComputeProgram> program, uint32_t x, uint32_t y, uint32_t z, bool async);
    void Dispatch(vk::CommandBuffer cmdBuffer, SafePtr<class ComputeProgram> program, uint32_t x, uint32_t y, uint32_t z);

    void Blit(vk::CommandBuffer cmdBuffer, SafePtr<class Texture> src, SafePtr<class Texture> dst);

    // TODO: move to a resource manager
    [[nodiscard]] SafePtr<class GfxPipeline> CreateGraphicsPipeline(const struct GraphicsPipelineDesc& createInfo);
    [[nodiscard]] SafePtr<class StorageBuffer> CreateGeometryBuffer(const void* data, size_t size);
    [[nodiscard]] SafePtr<class Texture> CreateTexture(const std::string& fullPath, vk::Format format = vk::Format::eR8G8B8A8Srgb);
    [[nodiscard]] SafePtr<class Texture> CreateCubemapTexture(const std::vector<std::string>& faces);
    [[nodiscard]] SafePtr<class WorldEnvironment> CreateEnvironmentMap(std::string_view pathToEnvMap, uint32_t dimensions = 1024);

    [[nodiscard]] SafePtr<class UniformBufferManager> RegisterObject();
    void AddTextureToUpdate(SafePtr<class Texture> texture);

private:
    SafePtr<class GfxContext> m_Context;
    SafePtr<class Swapchain> m_Swapchain;
    SafePtr<class GfxLoader> m_GfxLoader;
    std::shared_ptr<class enki::TaskScheduler> m_TaskScheduler;
    std::vector<SafePtr<class Texture>> m_TexturesToUpdate{};
    std::mutex m_TexturesToUpdateMutex{};

    // TODO: move to a command buffer manager to the context (maybe)
    std::vector<FrameData> m_FrameData;

    SafePtr<class GfxPipeline> m_LastUsedPipeline;
    SafePtr<class StaticMesh> m_LastUsedStaticMesh;

    SafePtr<class Texture> m_BRDFLut;

    bool m_LoadAsync{ true };

private:
    void InitFrameData(uint32_t index);
    void UpdateTextures(vk::CommandBuffer cmdBuffer);
};
}
