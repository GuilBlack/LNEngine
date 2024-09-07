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
namespace ResourceTypes
{
enum Enum : uint8_t
{
    eTexture,
    eCubemap,
};

enum Mask
{
    mTexture = 1 << 0,
    mCubemap = 1 << 1,
};

extern const char** s_Enum;
std::string_view ToString(Enum type);
}

struct UploadRequest
{
    ResourceTypes::Enum Type;
    SafePtr<class Texture> Texture;
    SafePtr<class StorageBuffer> Buffer;
    uint32_t Size;
    void* Data;
};

struct LoadRequest
{
    ResourceTypes::Enum Type{};
    SafePtr<class Texture> Texture{};
    SafePtr<class StorageBuffer> Buffer;
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

class GfxLoader : public RefCountBase
{
public:
    MOVABLE_ONLY(GfxLoader);
    GfxLoader() = default;
    ~GfxLoader() = default;

    void Init(class Renderer* renderer, SafePtr<class GfxContext> context, std::shared_ptr<class enki::TaskScheduler> scheduler);
    void Nuke();

    void Update();

    SafePtr<class Texture> CreateTexture(std::string_view fullPath);
    SafePtr<class Texture> CreateCubemap(std::vector<std::string> faces);

private:
    class Renderer* m_Renderer;
    SafePtr<class GfxContext> m_GraphicsContext;
    std::weak_ptr<enki::TaskScheduler> m_TaskScheduler;
    std::unique_ptr<GfxLoaderTask> m_GfxLoaderTask;

    std::vector<UploadRequest> m_GPUUploadRequests;
    std::mutex m_UploadRequestsMutex;
    std::vector<LoadRequest> m_LoadRequests;
    std::mutex m_LoadRequestsMutex;
    vk::Semaphore m_TransferSemaphore;
    
    BufferAllocation m_StagingBuffer;

    SafePtr<class Texture> m_ReadyTexture;

private:
    void ProcessUploadRequests();
    void ProcessLoadRequests();

    void LoadTexture(LoadRequest& request);
    void LoadCubemap(LoadRequest& request);
    void UploadTexture(UploadRequest& request);
};
}
