#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/GlobalGfxData.h"
#include "Engine/Resources/GfxLoader.h"
#include "Engine/GlobalUtils.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"
#include "../../vendor/ENKITS/enkiTS/src/TaskScheduler.h"
#include "Engine/Core/Utils/Defines.h"

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

constexpr u32 RENDER_THREAD_ID = 1;
using RenderTaskFunction = enki::PinnedTaskFunction;

struct RenderTask
{
    using InvokeFunc = void(*)(void*);

    InvokeFunc                                      Invoke;
    u32                                             Size;
};

struct RenderTasksLauncher : enki::ITaskSet
{
    //std::vector<RenderTaskFunction>*                m_pRenderTaskFuncs = nullptr;
    u8*                                             m_RenderTasksAllocation{};
    u64                                             m_RenderTasksAllocationOffset{};

    void                                            ExecuteRange(enki::TaskSetPartition range,
                                                                 u32 threadnum) override;
};

struct DrawMeshArgs
{
    SafePtr<StaticMesh>                             Mesh;
    SafePtr<StandaloneStorageBuffer>                TransformBuffer;
    SafePtr<StandaloneStorageBuffer>                LightsBuffer;
    PassID                                          PassId;
    u32                                             Offset;
    u32                                             SubMeshIndex;
    u32                                             InstanceCount;
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
    [[nodiscard]] u32                               GetCurrentFrameIndex() const { return m_CurrentFrameInFlight; }
    [[nodiscard]] u32                               GetCurrentFrameIndexOnMainThread() const { return m_CurrentFrameInFlightMain; }
    [[nodiscard]] u32                               GetCurrentSwapchainImageIndex() const { return m_CurrentSwapchainImageIndex; }
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
                                                         const DrawMeshArgs& drawArgs);

    void                                            DrawClassicMesh(vk::CommandBuffer cmdBuffer,
                                                                    const DrawMeshArgs& drawArgs,
                                                                    SafePtr<Material>& material);
    void                                            DrawMeshlets(vk::CommandBuffer cmdBuffer,
                                                                 const DrawMeshArgs& drawArgs,
                                                                 SafePtr<Material>& material);
    void                                            DrawFullscreenQuad(vk::CommandBuffer cmdBuffer,
                                                                        SafePtr<Material> material,
                                                                       const SafePtr<StandaloneStorageBuffer>& lightBuffer,
                                                                       PassID passId);

    void                                            Dispatch(SafePtr<class ComputeProgram> program, 
                                                             u32 x, u32 y, u32 z, 
                                                             bool async);
    void                                            Dispatch(vk::CommandBuffer cmdBuffer, 
                                                             SafePtr<class ComputeProgram> program, 
                                                             u32 x, u32 y, u32 z);

    // TODO: move to a resource manager
    [[nodiscard]] SafePtr<class GfxPipeline>        CreateGraphicsPipeline(const struct GraphicsPipelineDesc& createInfo);
    [[nodiscard]] SafePtr<class StorageBuffer>      CreateGeometryBuffer(const void* data, size_t size);
    [[nodiscard]] SafePtr<class Texture>            CreateTexture(const std::string& fullPath, 
                                                                  vk::Format format = vk::Format::eR8G8B8A8Srgb);

    [[nodiscard]] SafePtr<class Texture>            CreateCubemapTexture(const std::vector<std::string>& faces);
    [[nodiscard]] SafePtr<class WorldEnvironment>   CreateEnvironmentMap(std::string_view pathToEnvMap, 
                                                                         u32 dimensions = 1024);

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
    u32                                             m_PrevFrameInFlightMain{ 0 };
    u32                                             m_CurrentFrameInFlightMain{ 0 };
    std::atomic<u32>                                m_CurrentFrameInFlight{ 0 };
    std::atomic<u32>                                m_CurrentSwapchainImageIndex{ 0 };

    std::vector<u8*>                                m_FrameRenderTasksAllocation;
    std::vector<u64>                                m_FrameRenderTasksAllocationOffsets;
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
    void                                            InitFrameData(u32 index);
    void                                            UpdateTextures(CommandBuffer* cmdBuffer);

    // grows the material table bank for the effect
    void                                            ProcessDirtyEffects(vk::CommandBuffer cmdBuffer);

    // used after processing dirty materials because the material must know when it's safe
    // to update.
    void                                            CleanupDirtyEffects();
    void                                            ProcessDirtyMaterials(vk::CommandBuffer cmdBuffer);

    void*                                           AllocateRenderTask(RenderTask&& renderTask, u32 size);
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
