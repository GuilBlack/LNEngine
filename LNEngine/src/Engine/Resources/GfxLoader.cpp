#include <enkiTS/src/TaskScheduler.h>
#include <stb/stb_image.h>
#include <assimp/Importer.hpp>

#include "Core/Utils/Log.h"
#include "Graphics/Texture.h"
#include "Graphics/StorageBuffer.h"
#include "Graphics/GfxContext.h"
#include "Graphics/CommandBufferManager.h"
#include "Graphics/Renderer.h"
#include "Graphics/DynamicDescriptorAllocator.h"

#include "GfxLoader.h"

namespace lne
{
namespace ResourceTypes
{
const char* enumValues[2] = {
    "Texture",
    "Cubemap",
};

const char** s_Enum = enumValues;
std::string_view ToString(Enum type)
{
    return s_Enum[type];
}
}

void GfxLoaderTask::Execute()
{
    while (TaskScheduler.lock()->GetIsShutdownRequested() == false)
        Loader->Update();
}

void GfxLoader::Init(Renderer* renderer, SafePtr<class GfxContext> context, std::shared_ptr<enki::TaskScheduler> scheduler)
{
    m_Renderer = renderer;
    m_GraphicsContext = context;
    m_TaskScheduler = scheduler;

    m_LoadRequests.reserve(32);
    m_GPUUploadRequests.reserve(32);

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

    // create async task
    m_GfxLoaderTask.reset(lnnew GfxLoaderTask(scheduler, this));
    m_TaskScheduler.lock()->AddPinnedTask(m_GfxLoaderTask.get());
}

void GfxLoader::Nuke()
{
    m_GraphicsContext->FreeBuffer(m_StagingBuffer);
    m_GraphicsContext->GetDevice().destroySemaphore(m_TransferSemaphore);
    m_LoadRequests.clear();
    m_GPUUploadRequests.clear();
}

void GfxLoader::Update()
{
    if (m_ReadyTexture)
    {
        m_Renderer->AddTextureToUpdate(m_ReadyTexture);
        m_ReadyTexture.Reset();
    }

    ProcessLoadRequests();
    ProcessUploadRequests();
}

SafePtr<Texture> GfxLoader::CreateTexture(std::string_view fullPath)
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
    SafePtr<Texture> texture = Texture::CreateColorTexture2D(m_GraphicsContext, texWidth, texHeight, true, std::format("Texture: {}", fsFullPath.filename().string()));

    LoadRequest request;
    request.Type = ResourceTypes::eTexture;
    request.IsFile = true;
    request.Path.push_back(fullPath.data());
    request.Texture = texture;

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
    SafePtr<Texture> texture = Texture::CreateCubemapTexture(m_GraphicsContext, texWidth, texHeight, true,
        std::format("Texture: {}", fsFullPath.parent_path().filename().string()));

    LoadRequest request;
    request.Type = ResourceTypes::eCubemap;
    request.IsFile = true;
    request.Path = std::move(faces);
    request.Texture = texture;

    {
        std::lock_guard<std::mutex> lock(m_LoadRequestsMutex);
        m_LoadRequests.push_back(request);
    }

    return texture;
}

void GfxLoader::ProcessUploadRequests()
{
    auto& cbManager = m_GraphicsContext->GetTransferCommandBufferManager();
    auto device = m_GraphicsContext->GetDevice();

    if (m_GPUUploadRequests.empty())
        return;
    if (cbManager.GetFenceStatus(0) == false)
        return;

    cbManager.StartCommandBuffer(0);

    UploadRequest request;
    {
        std::lock_guard<std::mutex> lock(m_UploadRequestsMutex);
        request = m_GPUUploadRequests.back();
        m_GPUUploadRequests.pop_back();
    }

    switch (request.Type)
    {
    case ResourceTypes::eTexture:
    {
        UploadTexture(request);
        m_ReadyTexture = request.Texture;
        break;
    }
    case ResourceTypes::eCubemap:
    {
        UploadTexture(request);
        break;
    }
    default:
        LNE_ERROR("Doesn't support type {0} yet.", ResourceTypes::ToString(request.Type));
        break;
    }
    vk::PipelineStageFlags waitDst = vk::PipelineStageFlagBits::eTransfer;
    vk::SubmitInfo submitInfo{};
    submitInfo.pWaitDstStageMask = &waitDst;
    submitInfo.pWaitSemaphores = &m_TransferSemaphore;
    cbManager.Submit(submitInfo);
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
    {
        LoadTexture(request);
        break;
    }
    case ResourceTypes::eCubemap:
    {
        LoadCubemap(request);
        break;
    }
    default:
        LNE_ERROR("Doesn't support type {0} yet.", ResourceTypes::ToString(request.Type));
        break;
    }
}

void GfxLoader::LoadTexture(LoadRequest& request)
{
    auto& path = request.Path[0];
    int texWidth, texHeight, texChannels;
    Assimp::Importer importer;
    uint8_t* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
        LNE_ERROR("Failed to load texture image: {0}", path);

    UploadRequest gpuRequest;
    gpuRequest.Type = request.Type;
    gpuRequest.Texture = request.Texture;
    gpuRequest.Data = pixels;
    gpuRequest.Size = texWidth * texHeight * 4;

    {
        std::lock_guard<std::mutex> lock(m_UploadRequestsMutex);
        m_GPUUploadRequests.push_back(gpuRequest);
    }
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

    UploadRequest gpuRequest;
    gpuRequest.Type = request.Type;
    gpuRequest.Texture = request.Texture;
    gpuRequest.Data = allPixels;
    gpuRequest.Size = texWidth * texHeight * 4 * 6;

    {
        std::lock_guard<std::mutex> lock(m_UploadRequestsMutex);
        m_GPUUploadRequests.push_back(gpuRequest);
    }
}

void GfxLoader::UploadTexture(UploadRequest& request)
{
    auto& cbManager = m_GraphicsContext->GetTransferCommandBufferManager();
    auto& cmdBuffer = cbManager.GetCurrentCommandBuffer();

    request.Texture->UploadData(cmdBuffer, m_StagingBuffer, request.Data);
    if (request.Type == ResourceTypes::eTexture)
        stbi_image_free(request.Data);
    else
        delete[] request.Data;
}
}
