#pragma once
#include "../vendor/ENKITS/enkiTS/src/TaskScheduler.h"

#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/Utils/Defines.h"

namespace enki
{
class TaskScheduler;
}

namespace lne
{
class Renderer;
class GfxContext;
class CommandBuffer;
class enki::TaskScheduler;
class Texture;
class WorldEnvironment;
class ComputeProgram;

namespace ResourceTypes
{
enum Enum : u8
{
    eTexture,
    eCubemap,
    eEnvironment,
    eBuffer
};

extern const char** s_Enum;
std::string_view ToString(Enum type);
}

struct UploadRequest
{
    ResourceTypes::Enum Type{};
    SafePtr<class RefCountBase> Resource{};
    u32 Size{};
    void* Data{};
    glm::uvec3 Dimensions{ 0, 0, 0 };
    bool ShouldFreeData{true};
};

struct LoadRequest
{
    ResourceTypes::Enum Type{};
    SafePtr<class RefCountBase> Resource{};
    std::vector<std::string> Path{};
    void* Data{};
    bool IsFile{ true };
};

class GfxLoaderTask : public enki::IPinnedTask
{
public:
    GfxLoaderTask(std::shared_ptr<enki::TaskScheduler> TaskScheduler, class GfxLoader* Loader) 
        : TaskScheduler(TaskScheduler), Loader(Loader) 
    {
        threadNum = TaskScheduler->GetNumTaskThreads()-1;
    }

    virtual void Execute() override;

public:
    std::weak_ptr<enki::TaskScheduler> TaskScheduler;
    class GfxLoader* Loader;
};

struct GfxLoaderSettings
{
    Renderer* RendererParam{ nullptr };
    SafePtr<GfxContext> Context{};
    std::shared_ptr<enki::TaskScheduler> Scheduler{};
    bool LoadAsync{ true };
    u32 RadianceTextureMaxSize{ 512 };
};

class GfxLoader : public RefCountBase
{
public:
    MOVABLE_ONLY(GfxLoader);
    GfxLoader() = default;
    ~GfxLoader() = default;

    void Init(const GfxLoaderSettings& settings);
    void Nuke();

    void Update();

    [[nodiscard]] SafePtr<Texture>              CreateTexture(
        std::string_view fullPath, 
        vk::Format imageFormat = vk::Format::eR8G8B8A8Srgb);
    [[nodiscard]] SafePtr<Texture>              CreateCubemap(std::vector<std::string> faces);
    [[nodiscard]] SafePtr<WorldEnvironment>     CreateEnvironmentMap(std::string_view pathToEnvMap);
    void                                        InitStaticStorageBuffer(SafePtr<class StorageBuffer> buffer, const void* data);

    void                                        Upload(UploadRequest request)
    {
        std::lock_guard<std::mutex> lock(m_UploadRequestsMutex);
        m_UploadRequests.push_back(request);
    }

private:
    class Renderer* m_Renderer;
    SafePtr<GfxContext> m_GraphicsContext;
    std::weak_ptr<enki::TaskScheduler> m_TaskScheduler;
    std::unique_ptr<GfxLoaderTask> m_GfxLoaderTask;

    std::vector<UploadRequest> m_UploadRequests;
    std::mutex m_UploadRequestsMutex;
    std::vector<LoadRequest> m_LoadRequests;
    std::mutex m_LoadRequestsMutex;
    vk::Semaphore m_TransferSemaphore;
    
    BufferAllocation m_StagingBuffer;

    SafePtr<Texture> m_ReadyTexture;

    SafePtr<ComputeProgram> m_HDRToCubemapProgram;
    SafePtr<ComputeProgram> m_PrefilterProgram;
    SafePtr<ComputeProgram> m_IrradianceProgram;

    u32 m_RadianceTextureMaxSize;
    bool m_LoadAsync;
private:
    void ProcessUploadRequests();
    void ProcessLoadRequests();

    void LoadTexture(LoadRequest& request);
    void LoadCubemap(LoadRequest& request);
    void LoadEnvironment(LoadRequest& request);

    void UploadTexture(UploadRequest& request, CommandBuffer* cb);
    void UploadEnvironment(UploadRequest& request, CommandBuffer* cb);
    void UploadBuffer(UploadRequest& request, CommandBuffer* cb);
};
}
