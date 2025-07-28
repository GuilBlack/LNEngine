#include <enkiTS/src/TaskScheduler.h>
#include <stb/stb_image.h>
#include <assimp/Importer.hpp>

#include "Core/Utils/Log.h"
#include "Graphics/Texture.h"
#include "Graphics/StorageBuffer.h"
#include "Graphics/GfxContext.h"
#include "Graphics/CommandPoolManager.h"
#include "Graphics/Renderer.h"
#include "Graphics/DynamicDescriptorAllocator.h"
#include "Graphics/WorldEnvironment.h"

#include "GfxLoader.h"
#include "Core/ApplicationBase.h"

namespace lne
{
namespace ResourceTypes
{
const char* enumValues[4] = {
    "Texture",
    "Cubemap",
    "Environment",
    "Buffer"
};

const char** s_Enum = enumValues;
std::string_view ToString(Enum type)
{
    return s_Enum[type];
}
}

void GfxLoaderTask::Execute()
{
    tracy::SetThreadName("Graphics Loader");
    while (TaskScheduler.lock()->GetIsShutdownRequested() == false)
        Loader->Update();
}

void GfxLoader::Init(Renderer* renderer, SafePtr<class GfxContext> context, std::shared_ptr<enki::TaskScheduler> scheduler, bool loadAsync)
{
    m_LoadAsync = loadAsync;
    m_Renderer = renderer;
    m_GraphicsContext = context;
    m_TaskScheduler = scheduler;

    m_LoadRequests.reserve(32);
    m_UploadRequests.reserve(32);

    // allocate common staging buffer of 64MB
    vk::BufferCreateInfo bufferCI{
        {},
        64 * 1024 * 1024,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo allocCI{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    m_GraphicsContext->AllocateBuffer(m_StagingBuffer, bufferCI, allocCI);

    vk::SemaphoreCreateInfo semaphoreCI{};
    m_TransferSemaphore = m_GraphicsContext->GetDevice().createSemaphore(semaphoreCI);

    ComputePipelineDesc hdrToCubemapDesc;
    hdrToCubemapDesc.Name = "HDRToCubemap";
    hdrToCubemapDesc.PathToShader = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\Compute\\hdrToCubemap.comp";

    SafePtr hdrToCubemapPipeline = lnnew ComputePipeline(m_GraphicsContext, hdrToCubemapDesc);
    m_HDRToCubemapProgram = lnnew ComputeProgram(hdrToCubemapPipeline);
    // create async task
    m_GfxLoaderTask.reset(lnnew GfxLoaderTask(scheduler, this));

    if (m_LoadAsync)
        m_TaskScheduler.lock()->AddPinnedTask(m_GfxLoaderTask.get());
}

void GfxLoader::Nuke()
{
    m_GraphicsContext->FreeBufferAllocation(m_StagingBuffer);
    m_GraphicsContext->GetDevice().destroySemaphore(m_TransferSemaphore);
    m_LoadRequests.clear();
    m_UploadRequests.clear();
}

void GfxLoader::Update()
{
    if (m_ReadyTexture)
    {
        m_Renderer->AddTextureToUpdate(m_ReadyTexture);
        m_ReadyTexture.Reset();
    }

    ProcessUploadRequests();
    ProcessLoadRequests();
}

SafePtr<Texture> GfxLoader::CreateTexture(std::string_view fullPath, vk::Format imageFormat)
{
    int texWidth, texHeight, texChannels;
    if (stbi_info(fullPath.data(), &texWidth, &texHeight, &texChannels) == 0)
    {
        LNE_ERROR("Failed to load texture: {0}", fullPath);
        return SafePtr<Texture>();
    }
    stbi_info(fullPath.data(), &texWidth, &texHeight, &texChannels);

    std::filesystem::path fsFullPath = fullPath;
    // TODO: change mipmap gen to true when I'll implement the mipmap gen on the renderer side
    SafePtr<Texture> texture = Texture::CreateColorTexture2D(m_GraphicsContext, texWidth, texHeight, imageFormat, TextureUsageType::eSampled, true, std::format("Texture: {}", fsFullPath.filename().string()));

    LoadRequest request;
    request.Type = ResourceTypes::eTexture;
    request.IsFile = true;
    request.Path.push_back(fullPath.data());
    request.Resource = texture;

    {
        std::lock_guard<std::mutex> lock(m_LoadRequestsMutex);
        m_LoadRequests.push_back(request);
    }

    return texture;
}

SafePtr<Texture> GfxLoader::CreateCubemap(std::vector<std::string> faces)
{
    if (faces.size() != 6)
    {
        LNE_ERROR("Cubemap must have 6 faces");
        return SafePtr<Texture>();
    }

    int texWidth{}, texHeight{}, texChannels{};
    bool first = true;

    for (const auto& face : faces)
    {
        if (std::filesystem::exists(face) == false)
        {
            LNE_ERROR("Cubemap face not found: {0}", face);
            return SafePtr<Texture>();
        }
        int width, height, channels;

        if (stbi_info(face.c_str(), &width, &height, &channels) == 0)
        {
            LNE_ERROR("Failed to get info from cubemap face: {0}. The file format isn't supported", face);
            return SafePtr<Texture>();
        }

        if (first)
        {
            texWidth = width;
            texHeight = height;
            texChannels = channels;
            first = false;
            continue;
        }
        if (texWidth != width || texHeight != height || texChannels != channels)
        {
            LNE_ERROR("Cubemap faces have different dimensions or channels");
            return SafePtr<Texture>();
        }
    }
    texChannels = 4;

    std::filesystem::path fsFullPath = faces[0];
    SafePtr<Texture> texture = Texture::CreateCubemapTexture(
        m_GraphicsContext, texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, 
        TextureUsageType::eSampled, true,
        std::format("Texture: {}", fsFullPath.parent_path().filename().string())
    );

    LoadRequest request;
    request.Type = ResourceTypes::eCubemap;
    request.IsFile = true;
    request.Path = std::move(faces);
    request.Resource = texture;

    {
        std::lock_guard<std::mutex> lock(m_LoadRequestsMutex);
        m_LoadRequests.push_back(request);
    }

    return texture;
}

lne::SafePtr<class WorldEnvironment> GfxLoader::CreateEnvironmentMap(std::string_view pathToEnvMap)
{
    if (std::filesystem::exists(pathToEnvMap) == false || stbi_is_hdr(pathToEnvMap.data()) == false)
    {
        LNE_ERROR("Environment map is invalid: {0}", pathToEnvMap);
        return nullptr;
    }

    int texWidth, texHeight, texChannels;
    if (stbi_info(pathToEnvMap.data(), &texWidth, &texHeight, &texChannels) == 0)
    {
        LNE_ERROR("Failed to load environment map: {0}", pathToEnvMap);
        return nullptr;
    }
    uint32_t dimensions = texWidth / 4;
    // TODO: should I check if dimensions are power of two?
    if (dimensions != texHeight / 2 || texChannels != 3)
    {
        LNE_ERROR("Environment map must be a 4:2 equirectangular image with 3 channels (RGB)");
        return nullptr;
    }
    SafePtr<WorldEnvironment> env = SafePtr<WorldEnvironment>(lnnew WorldEnvironment());
    env->SkyboxTexture = Texture::CreateCubemapTexture(
        m_GraphicsContext, dimensions, dimensions, vk::Format::eR16G16B16A16Sfloat,
        TextureUsageType::eSampledAndStorage, false,
        std::format("Environment Radiance: {}", std::filesystem::path(pathToEnvMap).filename().string())
    );
    env->IrradianceTexture = Texture::CreateCubemapTexture(
        m_GraphicsContext, dimensions, dimensions, vk::Format::eR16G16B16A16Sfloat,
        TextureUsageType::eSampledAndStorage, false,
        std::format("Environment Irradiance: {}", std::filesystem::path(pathToEnvMap).filename().string())
    );

    LoadRequest request;
    request.Type = ResourceTypes::eEnvironment;
    request.IsFile = true;
    request.Path.push_back(pathToEnvMap.data());
    request.Resource = env;
    {
        std::lock_guard<std::mutex> lock(m_LoadRequestsMutex);
        m_LoadRequests.push_back(request);
    }

    return env;
}

void GfxLoader::InitStaticStorageBuffer(SafePtr<class StorageBuffer> buffer, const void* data)
{
    UploadRequest request{
        .Type = ResourceTypes::eBuffer,
        .Resource = buffer,
        .Size = (uint32_t)buffer->m_Size,
        .Data = const_cast<void*>(data),
        .ShouldFreeData = false,
    };

    {
        std::lock_guard<std::mutex> lock(m_LoadRequestsMutex);
        m_UploadRequests.push_back(request);
    }
}

void GfxLoader::ProcessUploadRequests()
{
    auto device = m_GraphicsContext->GetDevice();

    if (m_UploadRequests.empty())
        return;

    auto& cpManager = m_GraphicsContext->GetCommandPoolManager();
    vk::CommandBuffer cb = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Transfer);

    UploadRequest request = {};
    {
        std::lock_guard<std::mutex> lock(m_UploadRequestsMutex);
        request = m_UploadRequests.back();
        m_UploadRequests.pop_back();
    }

    switch (request.Type)
    {
    case ResourceTypes::eTexture:
        UploadTexture(request, cb);
        m_ReadyTexture = request.Resource;
        break;
    case ResourceTypes::eCubemap:
        UploadTexture(request, cb);
        m_ReadyTexture = request.Resource;
        break;
    case ResourceTypes::eEnvironment:
        UploadEnvironment(request, cb);
        break;
    case ResourceTypes::eBuffer:
        UploadBuffer(request, cb);
        break;
    default:
        LNE_ERROR("Doesn't support type {0} yet.", ResourceTypes::ToString(request.Type));
        break;
    }
    vk::PipelineStageFlags waitDst = vk::PipelineStageFlagBits::eTransfer;
    vk::SubmitInfo submitInfo{};
    cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Transfer, &waitDst, &m_TransferSemaphore);
}

void GfxLoader::ProcessLoadRequests()
{
    if (m_LoadRequests.empty())
        return;

    LoadRequest request;
    {
        std::lock_guard<std::mutex> lock(m_LoadRequestsMutex);
        request = m_LoadRequests.back();
        m_LoadRequests.pop_back();
    }

    switch (request.Type)
    {
    case ResourceTypes::eTexture:
        LoadTexture(request);
        break;
    case ResourceTypes::eCubemap:
        LoadCubemap(request);
        break;
    case ResourceTypes::eEnvironment:
        LoadEnvironment(request);
        break;
    default:
        LNE_ERROR("Doesn't support type {0} yet.", ResourceTypes::ToString(request.Type));
        break;
    }
}

void GfxLoader::LoadTexture(LoadRequest& request)
{
    auto& path = request.Path[0];
    int texWidth, texHeight, texChannels;
    uint8_t* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
        LNE_ERROR("Failed to load texture image: {0}", path);

    UploadRequest gpuRequest {};
    gpuRequest.Type = request.Type;
    gpuRequest.Resource = request.Resource;
    gpuRequest.Data = pixels;
    gpuRequest.Size = texWidth * texHeight * 4;

    Upload(gpuRequest);
}

void GfxLoader::LoadCubemap(LoadRequest& request)
{
    auto& facesPaths = request.Path;
    int texWidth{}, texHeight{}, texChannels{};
    stbi_info(facesPaths[0].c_str(), &texWidth, &texHeight, &texChannels);

    uint8_t* allPixels = lnnew uint8_t[texWidth * texHeight * 4 * 6];

    for (uint32_t i = 0; i < 6; ++i)
    {
        uint8_t* pixels = stbi_load(facesPaths[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels)
        {
            LNE_ERROR("Failed to load cubemap face: {0}", request.Path[i]);
            delete[] allPixels;
            return;
        }
        memcpy(allPixels + (texWidth * texHeight * texChannels * i), pixels, texWidth * texHeight * texChannels);
        stbi_image_free(pixels);
    }

    UploadRequest gpuRequest = {};
    gpuRequest.Type = request.Type;
    gpuRequest.Resource = request.Resource;
    gpuRequest.Data = allPixels;
    gpuRequest.Size = texWidth * texHeight * 4 * 6;

    {
        std::lock_guard<std::mutex> lock(m_UploadRequestsMutex);
        m_UploadRequests.push_back(gpuRequest);
    }
}

void GfxLoader::LoadEnvironment(LoadRequest& request)
{
    auto& path = request.Path[0];
    int texWidth, texHeight, texChannels;

    float* pixels = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        LNE_ERROR("Failed to load environment map image: {0}", path);
        return;
    }

    UploadRequest gpuRequest{};
    gpuRequest.Type = request.Type;
    gpuRequest.Resource = request.Resource;
    gpuRequest.Data = pixels;
    gpuRequest.Size = texWidth * texHeight * 4 * 4;
    gpuRequest.ShouldFreeData = true;
    gpuRequest.Dimensions = glm::uvec3(texWidth, texHeight, 1);
    Upload(gpuRequest);
}

void GfxLoader::UploadTexture(UploadRequest& request, vk::CommandBuffer cb)
{
    request.Resource.GetAs<Texture>()->UploadData(cb, m_StagingBuffer, request.Data);

    if (request.ShouldFreeData == false)
        return;

    if (request.Type == ResourceTypes::eTexture || request.Type == ResourceTypes::eEnvironment)
        stbi_image_free(request.Data);
    else
        delete[] request.Data;
}

void GfxLoader::UploadEnvironment(UploadRequest& request, vk::CommandBuffer cb)
{
    auto& renderer = ApplicationBase::GetRenderer();
    SafePtr<WorldEnvironment> env = request.Resource.GetAs<WorldEnvironment>();

    // 1) equirectangular to cubemap conversion
    SafePtr<Texture> hdrSource = Texture::CreateColorTexture2D(
        m_GraphicsContext, request.Dimensions.x, request.Dimensions.y,
        vk::Format::eR32G32B32A32Sfloat, TextureUsageType::eSampledAndStorage, false,
        std::format("Environment Source")
    );
    vk::CommandBuffer cbComp = m_GraphicsContext->GetCommandPoolManager().BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Compute);

    hdrSource->TransitionLayout(cbComp, vk::ImageLayout::eTransferDstOptimal);
    hdrSource->UploadData(cbComp, m_StagingBuffer, request.Data, request.Size, false);
    hdrSource->TransitionLayout(cbComp, vk::ImageLayout::eGeneral);
    
    env->SkyboxTexture->TransitionLayout(cbComp, vk::ImageLayout::eGeneral);

    m_HDRToCubemapProgram->SetTexture("tHDRTexture", hdrSource, false);
    m_HDRToCubemapProgram->SetTexture("tCubemapTexture", env->SkyboxTexture, true);

    renderer.Dispatch(cbComp, m_HDRToCubemapProgram, env->SkyboxTexture->GetDimensions().width / 16, env->SkyboxTexture->GetDimensions().height / 16, 6);

    m_GraphicsContext->GetCommandPoolManager().EndSingleUseCommandBuffer(EQueueFamilyType::Compute);

    if (request.ShouldFreeData)
        stbi_image_free(request.Data);
}

void GfxLoader::UploadBuffer(UploadRequest& request, vk::CommandBuffer cb)
{
    SafePtr buffer = request.Resource.GetAs<StorageBuffer>();
    buffer->m_StagingAllocation = m_GraphicsContext->AllocateStagingBuffer(buffer->m_Size);
    memcpy(buffer->m_StagingAllocation.AllocationInfo.pMappedData, request.Data, buffer->m_Size);

    vk::BufferCopy copyRegion = vk::BufferCopy{
        0,
        0,
        buffer->m_Size
    };

    cb.copyBuffer(buffer->m_StagingAllocation.Buffer, buffer->m_Allocation.Buffer, copyRegion);
}

}
