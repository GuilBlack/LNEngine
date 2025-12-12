#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/GlobalGfxData.h"
#include "Engine/Resources/GfxLoader.h"
#include "Engine/GlobalUtils.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"
#include "../../vendor/ENKITS/enkiTS/src/TaskScheduler.h"

namespace enki
{
class TaskScheduler;
class ICompletable;
}

namespace lne
{
class StorageBuffer;
class StaticMesh;
struct SubMesh;
class GfxContext;
class Shader;
class Effect;
class GfxTechnique;
struct GfxTechniqueDesc;
class GfxPipeline;
class Material;
class Texture;
class ComputeProgram;
class Framebuffer;
class Swapchain;
class WorldRenderer;
class FrameGraph;
class StandaloneStorageBuffer;

constexpr uint32_t RENDER_THREAD_ID = 1;
using RenderTaskFunction = enki::PinnedTaskFunction;

struct RenderTask
{
    using InvokeFunc = void(*)(void*);

    InvokeFunc                                      Invoke;
    uint32_t                                        Size;
};

struct RenderTasksLauncher : enki::ITaskSet
{
    //std::vector<RenderTaskFunction>*                m_pRenderTaskFuncs = nullptr;
    uint8_t*                                        m_RenderTasksAllocation{};
    uint64_t                                        m_RenderTasksAllocationOffset{};

    void                                            ExecuteRange(enki::TaskSetPartition range,
                                                                 uint32_t threadnum) override;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void                                            Init(std::unique_ptr<class Window>& window,
                                                         std::shared_ptr<enki::TaskScheduler> taskScheduler);
    void                                            Nuke();
    void                                            InitResources();
    void                                            NukeResources();

    // Gets the current frame index on the render thread.
    [[nodiscard]] uint32_t                          GetCurrentFrameIndex() const { return m_CurrentFrameInFlight; }
    [[nodiscard]] uint32_t                          GetCurrentFrameIndexOnMainThread() const { return m_CurrentFrameInFlightMain; }
    [[nodiscard]] uint32_t                          GetCurrentSwapchainImageIndex() const { return m_CurrentSwapchainImageIndex; }
    [[nodiscard]] SafePtr<class GfxContext>         GetGfxContext() const;
    [[nodiscard]] SafePtr<class GfxLoader>          GetGfxLoader() const;

    void                                            PushLabel(vk::CommandBuffer cmdBuffer, 
                                                              std::string_view label) const;
    void                                            PopLabel(vk::CommandBuffer cmdBuffer) const;

    void                                            BeginFrame();
    void                                            EndFrame();
    void                                            PostFrame();

    void                                            BeginScene(SafePtr<WorldRenderer> worldRenderer,
                                                               SafePtr<FrameGraph> frameGraph,
                                                               WorldData globalData,
                                                               SafePtr<class UniformBuffer> worldGlobalUniforms);

    void                                            BeginRenderPass(const class Framebuffer& framebuffer);
    void                                            EndRenderPass(const class Framebuffer& framebuffer);

    void                                            Draw(vk::CommandBuffer cmdBuffer,
                                                         const SafePtr<StaticMesh>& mesh,
                                                         const SafePtr<StandaloneStorageBuffer>& transformBuffer,
                                                         PassID passId,
                                                         uint32_t offset, uint32_t subMeshIndex,
                                                         uint32_t instanceCount);

    void                                            DrawClassicMesh(vk::CommandBuffer cmdBuffer,
                                                                    const SafePtr<StaticMesh>& mesh, const SubMesh& subMesh,
                                                                    SafePtr<Material> material,
                                                                    SafePtr<Effect> effect, SafePtr<GfxPipeline> pipeline,
                                                                    const SafePtr<StandaloneStorageBuffer>& transformBuffer,
                                                                    uint32_t offset, uint32_t instanceCount,
                                                                    PassID passId);
    void                                            DrawMeshlets(vk::CommandBuffer cmdBuffer,
                                                                 const SafePtr<StaticMesh>& mesh, const SubMesh& subMesh,
                                                                 SafePtr<Material> material,
                                                                 SafePtr<Effect> effect, SafePtr<GfxPipeline> pipeline,
                                                                 const SafePtr<StandaloneStorageBuffer>& transformBuffer,
                                                                 uint32_t offset, uint32_t instanceCount,
                                                                 PassID passId);

    void                                            DrawFullscreenQuad(vk::CommandBuffer cmdBuffer,
                                                                        SafePtr<Material> material,
                                                                       PassID passId);


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

    SafePtr<Shader>                                 CreateOrGetShader(const std::string& path);
    SafePtr<Effect>                                 CreateOrGetEffect(const std::string& path);
    SafePtr<GfxTechnique>                           CreateOrGetTechnique(const GfxTechniqueDesc& techniqueDesc);
    [[nodiscard]] SafePtr<GfxTechnique>             GetTechnique(const std::string& name);

    void                                            AddTextureToUpdate(SafePtr<class Texture> texture);


    void                                            AddShaderIncludeDir(const std::filesystem::path& dir)
    {
        std::lock_guard<std::mutex> lock(m_ShaderIncludeDirsMutex);
        m_ShaderInudeDirs.push_back(dir);
    }

    void                                            AddDirtyEffect(SafePtr<class Effect> effect);
    void                                            AddDirtyMaterial(SafePtr<class Material> material);

    [[nodiscard]] std::vector<std::filesystem::path> GetShaderIncludeDirs()
    {
        std::lock_guard<std::mutex> lock(m_ShaderIncludeDirsMutex);
        return m_ShaderInudeDirs;
    }

    [[nodiscard]] SafePtr<Texture>                  GetBRDFLut() const;
    [[nodiscard]] SafePtr<Texture>                  GetDefaultTexture() const;
    [[nodiscard]] SafePtr<Texture>                  GetWhiteTexture() const;
    [[nodiscard]] std::filesystem::path             GetShaderCachePath() const;

    template<typename RenderTaskLambda>
    void                                            AddRenderTask(RenderTaskLambda&& renderTaskLambda);

    bool                                            IsAsync() { return m_IsAsync; }
    void                                            RunRenderTasks();
    void WaitForRenderTasksToFinish();

private:
    SafePtr<GfxContext>                             m_Context;
    SafePtr<Swapchain>                              m_Swapchain;
    SafePtr<GfxLoader>                              m_GfxLoader;
    std::shared_ptr<enki::TaskScheduler>            m_TaskScheduler;
    std::vector<SafePtr<Texture>>                   m_TexturesToUpdate{};
    std::mutex                                      m_TexturesToUpdateMutex{};
    std::vector<SafePtr<Effect>>                    m_DirtyEffects{};
    std::mutex                                      m_DirtyEffectsMutex{};
    std::vector<SafePtr<Material>>                  m_DirtyMaterials{};
    std::mutex                                      m_DirtyMaterialsMutex{};
    uint32_t                                        m_PrevFrameInFlightMain{ 0 };
    uint32_t                                        m_CurrentFrameInFlightMain{ 0 };
    std::atomic<uint32_t>                           m_CurrentFrameInFlight{ 0 };
    std::atomic<uint32_t>                           m_CurrentSwapchainImageIndex{ 0 };

    std::vector<uint8_t*>                           m_FrameRenderTasksAllocation;
    std::vector<uint64_t>                           m_FrameRenderTasksAllocationOffsets;
    std::vector<RenderTasksLauncher*>               m_RenderTasksLauncher;

    std::vector<FrameData>                          m_FrameData;

    SafePtr<GfxPipeline>                            m_LastUsedPipeline;
    SafePtr<Effect>                                 m_LastUsedEffect;
    SafePtr<StaticMesh>                             m_LastUsedStaticMesh;

    // TODO: change this for multiple world renderers for later
    SafePtr<WorldRenderer>                          m_CurrentWorldRenderer;
    SafePtr<FrameGraph>                             m_CurrentFrameGraph;

    SafePtr<Texture>                                m_BRDFLut;

    std::mutex                                      m_ShaderIncludeDirsMutex;
    std::vector<std::filesystem::path>              m_ShaderInudeDirs;

    // TODO: move to a resource manager
    FlatHashMap<std::string, SafePtr<Shader>>       m_ShadersLibrary;
    std::mutex                                      m_ShadersLibraryMutex;
    FlatHashMap<std::string, SafePtr<Effect>>       m_EffectsLibrary;
    std::mutex                                      m_EffectsLibraryMutex;
    FlatHashMap<std::string, SafePtr<GfxTechnique>> m_TechniquesLibrary;
    std::mutex                                      m_TechniquesLibraryMutex;

    bool m_IsAsync{ true };

private:
    void                                            InitFrameData(uint32_t index);
    void                                            UpdateTextures(vk::CommandBuffer cmdBuffer);

    // grows the material table bank for the effect
    void                                            ProcessDirtyEffects(vk::CommandBuffer cmdBuffer);

    // used after processing dirty materials because the material must know when it's safe
    // to update.
    void                                            CleanupDirtyEffects();
    void                                            ProcessDirtyMaterials(vk::CommandBuffer cmdBuffer);

    void*                                           AllocateRenderTask(RenderTask&& renderTask, uint32_t size);
};

template<typename RenderTaskLambda>
void Renderer::AddRenderTask(RenderTaskLambda&& renderTask)
{
    using StoredT = std::decay_t<RenderTaskLambda>;
    void* allocation = AllocateRenderTask(RenderTask{
        .Invoke = [](void* data)
        {
            auto& rt = *static_cast<StoredT*>(data);
            rt();
            if constexpr (!std::is_trivially_destructible_v<StoredT>)
                rt.~StoredT();
        },
        .Size = sizeof(StoredT)
    }, sizeof(StoredT));
    new (allocation) StoredT(std::forward<RenderTaskLambda>(renderTask));
}
}
