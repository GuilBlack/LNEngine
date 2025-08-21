#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/GlobalGfxData.h"
#include "Engine/Resources/GfxLoader.h"

namespace enki
{
class TaskScheduler;
}

namespace lne
{
class StorageBuffer;
class StaticMesh;
class GfxContext;
class GfxPipeline;
class Material;
class Texture;
class ComputeProgram;
class Framebuffer;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void                                            Init(std::unique_ptr<class Window>& window,
                                                         std::shared_ptr<enki::TaskScheduler> taskScheduler);
    void                                            Nuke();
    void                                            InitResources();

    [[nodiscard]] uint32_t                          GetCurrentFrameIndex() const;
    [[nodiscard]] SafePtr<class GfxContext>         GetGfxContext() const;
    [[nodiscard]] SafePtr<class GfxLoader>          GetGfxLoader() const;

    void                                            PushLabel(vk::CommandBuffer cmdBuffer, 
                                                              std::string_view label) const;
    void                                            PopLabel(vk::CommandBuffer cmdBuffer) const;

    void                                            BeginFrame();
    void                                            EndFrame();
    void                                            PostFrame();

    void                                            BeginScene(WorldData globalData, 
                                                               SafePtr<class UniformBuffer> worldGlobalUniforms);

    void                                            BeginRenderPass(const class Framebuffer& framebuffer) const;
    void                                            EndRenderPass(const class Framebuffer& framebuffer) const;


    void                                            Draw(vk::CommandBuffer cmdBuffer, 
                                                         const SafePtr<class StaticMesh>& mesh, 
                                                         const SafePtr<class StandaloneStorageBuffer>& transformBuffer,
                                                         uint32_t offset, uint32_t subMeshIndex, uint32_t instanceCount);

    void                                            Draw(vk::CommandBuffer cmdBuffer, 
                                                         const SafePtr<StaticMesh>& mesh, 
                                                         const SafePtr<StandaloneStorageBuffer>& transformBuffer,
                                                         SafePtr<Material> overrideMaterial, 
                                                         uint32_t offset, uint32_t subMeshIndex, uint32_t instanceCount);


    void                                            DrawFullscreenQuad(vk::CommandBuffer cmdBuffer, 
                                                                       const SafePtr<class Material>& material);


    void                                            Dispatch(SafePtr<class ComputeProgram> program, 
                                                             uint32_t x, uint32_t y, uint32_t z, 
                                                             bool async);

    void                                            Dispatch(vk::CommandBuffer cmdBuffer, 
                                                             SafePtr<class ComputeProgram> program, 
                                                             uint32_t x, uint32_t y, uint32_t z);

    void                                            Blit(vk::CommandBuffer cmdBuffer, 
                                                         SafePtr<class Texture> src, 
                                                         SafePtr<class Texture> dst);


    // TODO: move to a resource manager
    [[nodiscard]] SafePtr<class GfxPipeline>        CreateGraphicsPipeline(const struct GraphicsPipelineDesc& createInfo);
    [[nodiscard]] SafePtr<class StorageBuffer>      CreateGeometryBuffer(const void* data, size_t size);
    [[nodiscard]] SafePtr<class Texture>            CreateTexture(const std::string& fullPath, 
                                                                  vk::Format format = vk::Format::eR8G8B8A8Srgb);

    [[nodiscard]] SafePtr<class Texture>            CreateCubemapTexture(const std::vector<std::string>& faces);
    [[nodiscard]] SafePtr<class WorldEnvironment>   CreateEnvironmentMap(std::string_view pathToEnvMap, 
                                                                         uint32_t dimensions = 1024);


    void                                            AddTextureToUpdate(SafePtr<class Texture> texture);


    void                                            AddShaderIncludeDir(const std::filesystem::path& dir)
    {
        std::lock_guard<std::mutex> lock(m_ShaderIncludeDirsMutex);
        m_ShaderInudeDirs.push_back(dir);
    }

    [[nodiscard]] std::vector<std::filesystem::path> GetShaderIncludeDirs()
    {
        std::lock_guard<std::mutex> lock(m_ShaderIncludeDirsMutex);
        return m_ShaderInudeDirs;
    }

    [[nodiscard]] SafePtr<class Texture>            GetBRDFLut() const;
    [[nodiscard]] std::filesystem::path             GetShaderCachePath() const;

private:
    SafePtr<class GfxContext>                   m_Context;
    SafePtr<class Swapchain>                    m_Swapchain;
    SafePtr<class GfxLoader>                    m_GfxLoader;
    std::shared_ptr<class enki::TaskScheduler>  m_TaskScheduler;
    std::vector<SafePtr<class Texture>>         m_TexturesToUpdate{};
    std::mutex                                  m_TexturesToUpdateMutex{};

    // TODO: move to a command buffer manager to the context (maybe)
    std::vector<struct FrameData>               m_FrameData;

    SafePtr<class GfxPipeline>                  m_LastUsedPipeline;
    SafePtr<class StaticMesh>                   m_LastUsedStaticMesh;

    SafePtr<class Texture>                      m_BRDFLut;

    std::mutex                                  m_ShaderIncludeDirsMutex;
    std::vector<std::filesystem::path>          m_ShaderInudeDirs;

    bool m_LoadAsync{ true };

private:
    void InitFrameData(uint32_t index);
    void UpdateTextures(vk::CommandBuffer cmdBuffer);
};
}
