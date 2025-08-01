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

void GfxLoader::Init(const GfxLoaderSettings& settings)
{
    LNE_ASSERT(settings.RendererParam, "Renderer is not set in GfxLoaderSettings");
    LNE_ASSERT(settings.Context, "GfxContext is not set in GfxLoaderSettings");
    LNE_ASSERT(settings.Scheduler, "TaskScheduler is not set in GfxLoaderSettings");
    m_LoadAsync = settings.LoadAsync;
    m_Renderer = settings.RendererParam;
    m_GraphicsContext = settings.Context;
    m_TaskScheduler = settings.Scheduler;
    m_RadianceTextureMaxSize = settings.RadianceTextureMaxSize;

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
    hdrToCubemapDesc.PathToShader = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\Compute\\HDRToCubemap.comp";
    SafePtr hdrToCubemapPipeline = lnnew ComputePipeline(m_GraphicsContext, hdrToCubemapDesc);
    m_HDRToCubemapProgram = lnnew ComputeProgram(hdrToCubemapPipeline);

    ComputePipelineDesc prefilterProgramDesc;
    prefilterProgramDesc.Name = "PrefilterCube";
    prefilterProgramDesc.PathToShader = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\Compute\\PrefilterCube.comp";
    SafePtr prefilterPipeline = lnnew ComputePipeline(m_GraphicsContext, prefilterProgramDesc);
    m_PrefilterProgram = lnnew ComputeProgram(prefilterPipeline);

    ComputePipelineDesc irradianceProgramDesc;
    irradianceProgramDesc.Name = "IrradianceCube";
    irradianceProgramDesc.PathToShader = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\Compute\\IrradianceCube.comp";
    SafePtr irradiancePipeline = lnnew ComputePipeline(m_GraphicsContext, irradianceProgramDesc);
    m_IrradianceProgram = lnnew ComputeProgram(irradiancePipeline);

    // create async task
    m_GfxLoaderTask.reset(lnnew GfxLoaderTask(settings.Scheduler, this));

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
        TextureUsageType::eSampledAndStorage, true,
        std::format("Environment Skybox: {}", std::filesystem::path(pathToEnvMap).filename().string())
    );

    env->IrradianceTexture = Texture::CreateCubemapTexture(
        m_GraphicsContext, 64, 64, vk::Format::eR16G16B16A16Sfloat,
        TextureUsageType::eSampledAndStorage, true,
        std::format("Environment Irradiance: {}", std::filesystem::path(pathToEnvMap).filename().string())
    );
    uint32_t radianceDim = std::min(m_RadianceTextureMaxSize, dimensions);
    env->PrefilteredTexture = Texture::CreateCubemapTexture(
        m_GraphicsContext, radianceDim, radianceDim, vk::Format::eR16G16B16A16Sfloat,
        TextureUsageType::eSampledAndStorage, true,
        std::format("Environment Radiance: {}", std::filesystem::path(pathToEnvMap).filename().string())
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

void GfxLoader::UploadEnvironment(UploadRequest& request, vk::CommandBuffer cmdBuffer)
{
    auto& renderer = ApplicationBase::GetRenderer();
    SafePtr<WorldEnvironment> env = request.Resource.GetAs<WorldEnvironment>();

    // 1) equirectangular to cubemap conversion
    SafePtr<Texture> hdrSource = Texture::CreateColorTexture2D(
        m_GraphicsContext, request.Dimensions.x, request.Dimensions.y,
        vk::Format::eR32G32B32A32Sfloat, TextureUsageType::eSampledAndStorage, false,
        std::format("Environment Source")
    );
    auto& cpManager = m_GraphicsContext->GetCommandPoolManager();

    // convert the HDR source to cubemap in the skybox texture & radiance texture
    vk::CommandBuffer singleUseBuffer = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Compute);

    hdrSource->TransitionLayout(singleUseBuffer, vk::ImageLayout::eTransferDstOptimal);
    hdrSource->UploadData(singleUseBuffer, m_StagingBuffer, request.Data, request.Size, false);
    hdrSource->TransitionLayout(singleUseBuffer, vk::ImageLayout::eGeneral);
    
    env->SkyboxTexture->TransitionLayout(singleUseBuffer, vk::ImageLayout::eGeneral);

    m_HDRToCubemapProgram->SetTexture("tHDRTexture", hdrSource, false);
    m_HDRToCubemapProgram->SetTexture("tCubemapTexture", env->SkyboxTexture, true);
    renderer.Dispatch(singleUseBuffer, m_HDRToCubemapProgram, env->SkyboxTexture->GetDimensions().width / 16, env->SkyboxTexture->GetDimensions().height / 16, 6);

    cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Compute);
    singleUseBuffer = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Compute);

    env->PrefilteredTexture->TransitionLayout(singleUseBuffer, vk::ImageLayout::eGeneral);
    m_HDRToCubemapProgram->SetTexture("tCubemapTexture", env->PrefilteredTexture, true);
    renderer.Dispatch(singleUseBuffer, m_HDRToCubemapProgram, env->PrefilteredTexture->GetDimensions().width / 16, env->PrefilteredTexture->GetDimensions().height / 16, 6);

    cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Compute);

    // generate skybox mipmaps
    singleUseBuffer = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Graphics);
    env->SkyboxTexture->GenerateMipmaps(singleUseBuffer);
    cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Graphics);

    // generate radiance prefiltered mipmaps
    uint32_t numMips = env->PrefilteredTexture->GetMipLevels();
    struct TempImageView
    {
        vk::ImageView ImageView;
        BindlessImageHandle BindlessTextureHandle;
    };
    std::vector<TempImageView> tempImageViews;
    tempImageViews.reserve(numMips);
    for (uint32_t i = 1; i < numMips; ++i)
    {
        vk::ImageView view = env->PrefilteredTexture->CreateImageViewForMip(i);
        BindlessImageHandle imageHandle = m_GraphicsContext->RegisterBindlessImage(view);
        tempImageViews.emplace_back(TempImageView{
            .ImageView = view,
            .BindlessTextureHandle = imageHandle
        });
    }

    singleUseBuffer = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Compute);
    
    std::vector<SafePtr<ComputeProgram>> programs;
    programs.reserve(numMips - 2);
    for (uint32_t i = 1; i < numMips; ++i)
    {
        float roughness = (float)i / (float)(numMips - 1);
        SafePtr program = lnnew ComputeProgram(m_PrefilterProgram->GetPipeline());
        program->SetTexture("tRadianceCubemap", env->SkyboxTexture, false);
        program->SetProperty("tPrefilteredCubemap", tempImageViews[i - 1].BindlessTextureHandle);
        program->SetProperty("uRoughness", roughness);
        program->SetProperty("uNumSamples", 1024u);
        uint32_t dim = env->PrefilteredTexture->GetDimensions().width >> i;
        uint32_t numGroups = (dim + 31) / 32;
        renderer.Dispatch(singleUseBuffer, program, numGroups, numGroups, 6);
        programs.push_back(program);
    }

    std::vector<TempImageView> tempImageViews2;
    numMips = env->IrradianceTexture->GetMipLevels();
    for (uint32_t i = 0; i < numMips; ++i)
    {
        vk::ImageView view = env->IrradianceTexture->CreateImageViewForMip(i);
        BindlessImageHandle imageHandle = m_GraphicsContext->RegisterBindlessImage(view);
        tempImageViews2.emplace_back(TempImageView{
            .ImageView = view,
            .BindlessTextureHandle = imageHandle
        });
    }
    env->IrradianceTexture->TransitionLayout(singleUseBuffer, vk::ImageLayout::eGeneral);
    std::vector<SafePtr<ComputeProgram>> irradiancePrograms;
    for (uint32_t i = 0; i < numMips; ++i)
    {
        SafePtr program = lnnew ComputeProgram(m_IrradianceProgram->GetPipeline());
        program->SetTexture("tRadianceCubemap", env->SkyboxTexture, false);
        program->SetProperty("tIrradianceCubemap", tempImageViews2[i].BindlessTextureHandle);
        program->SetProperty("uPhiDelta", 0.025f);
        program->SetProperty("uThetaDelta", 0.025f);
        uint32_t dim = env->IrradianceTexture->GetDimensions().width >> i;
        uint32_t numGroups = (dim + 31) / 32;
        renderer.Dispatch(singleUseBuffer, program, numGroups, numGroups, 6);
        irradiancePrograms.push_back(program);
    }

    cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Compute);

    for (auto& tempView : tempImageViews)
    {
        m_GraphicsContext->EnqueueResourceDeletion(
            ResourceDeletion{
                .Type = ResourceType::Enum::eImageView,
                .Resource = ImageViewDeletion{
                    .ImageView = tempView.ImageView,
                    .UsageType = TextureUsageType::eStorage,
                    .BindlessTextureHandle = tempView.BindlessTextureHandle
                },
                .ElapsedFrames = 1
            }
        );
    }

    for (auto& tempView : tempImageViews2)
    {
        m_GraphicsContext->EnqueueResourceDeletion(
            ResourceDeletion{
                .Type = ResourceType::Enum::eImageView,
                .Resource = ImageViewDeletion{
                    .ImageView = tempView.ImageView,
                    .UsageType = TextureUsageType::eStorage,
                    .BindlessTextureHandle = tempView.BindlessTextureHandle
                },
                .ElapsedFrames = 1
            }
        );
    }

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
