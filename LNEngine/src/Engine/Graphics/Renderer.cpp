#include "enkiTS/src/TaskScheduler.h"
#include "Renderer.h"
#include "Core/Utils/Log.h"
#include "Core/Window.h"
#include "Core/Utils/Defines.h"
#include "Core/Utils/Profiling.h"
#include "Resources/GfxLoader.h"
#include "Scene/Components.h"
#include "Graphics/CommandPoolManager.h"
#include "Graphics/Framebuffer.h"
#include "Graphics/GfxContext.h"
#include "Graphics/DynamicDescriptorAllocator.h"
#include "Graphics/Resources/Mesh.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/Texture.h"
#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/StorageBuffer.h"
#include "Graphics/Resources/GfxTechnique.h"
#include "Graphics/FrameGraph/FrameGraph.h"

// TODO: move this to a resource manager
#include <stb/stb_image.h>
#include "WorldEnvironment.h"
#include "Core/ApplicationBase.h"
#include "Resources/Effect.h"
#include "WorldRenderer.h"

namespace lne
{
#define PROFILING_COL 0xFF5B53

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Init(std::unique_ptr<Window>& window, std::shared_ptr<enki::TaskScheduler> taskScheduler)
{
    m_Context = window->GetGfxContext();
    m_Swapchain = window->GetSwapchain();
    m_TaskScheduler = taskScheduler;
    m_LoadAsync = true;
    AddShaderIncludeDir(ApplicationBase::GetAssetsPath() + "Engine/Shaders/Includes");
    std::filesystem::path shaderCachePath = GetShaderCachePath();
    if (!std::filesystem::exists(shaderCachePath))
        std::filesystem::create_directories(shaderCachePath);

    m_GfxLoader = lnnew GfxLoader();
    GfxLoaderSettings gfxLoaderSettings{
        .RendererParam = this,
        .Context = m_Context,
        .Scheduler = m_TaskScheduler,
        .LoadAsync = m_LoadAsync,
        .RadianceTextureMaxSize = 512
    };

    m_GfxLoader->Init(gfxLoaderSettings);
    m_TexturesToUpdate.reserve(128);
    for (uint32_t i = 0; i < m_Context->GetMaxFramesInFlight(); i++)
    {
        InitFrameData(i);
    }
}

void Renderer::Nuke()
{
    m_Context->WaitIdle();
    m_GfxLoader->Nuke();
    for (auto& frameData : m_FrameData)
    {
        frameData.DescriptorAllocator.Reset();
        m_Context->GetDevice().destroyDescriptorSetLayout(frameData.DescriptorSetLayout);
    }
    m_FrameData.clear();
    m_GfxLoader.Reset();
    m_Swapchain.Reset();
    m_Context.Reset();
}

void Renderer::InitResources()
{
    m_BRDFLut = Texture::CreateColorTexture2D(m_Context, 512, 512, vk::Format::eR16G16Sfloat, TextureUsageType::eSampledAndStorage, false, "BRDFLut");
    vk::CommandBuffer cmdBuffer = m_Context->GetCommandPoolManager().BeginOrGetPrimaryFrameCommandBuffer(m_Context->GetCurrentFrameIndex());
    m_BRDFLut->TransitionLayout(cmdBuffer, vk::ImageLayout::eGeneral);

    ComputePipelineDesc desc{};
    desc.Name = "GenerateBRDFLut";
    desc.PathToShader = ApplicationBase::GetAssetsPath() + "Engine/Shaders/Compute/GenerateBRDFLut.comp";
    SafePtr brdfPipeline = lnnew ComputePipeline(m_Context, desc);
    SafePtr brdfProgram = lnnew ComputeProgram(brdfPipeline);
    brdfProgram->SetProperty("uNumSamples", 1024);
    brdfProgram->SetTexture("tBRDFLut", m_BRDFLut, true);
    Dispatch(cmdBuffer, brdfProgram, 512 / 32, 512 / 32, 1);

    m_BRDFLut->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Renderer::NukeResources()
{
    m_LastUsedPipeline.Reset();
    m_LastUsedStaticMesh.Reset();
    m_CurrentWorldRenderer.Reset();
    m_CurrentFrameGraph.Reset();
    m_ShadersLibrary.clear();
    m_EffectsLibrary.clear();
    m_TechniquesLibrary.clear();
}

uint32_t Renderer::GetCurrentFrameIndex() const
{
    return m_Context->GetCurrentFrameIndex();
}

lne::SafePtr<class GfxContext> Renderer::GetGfxContext() const
{
    return m_Context;
}

lne::SafePtr<class GfxLoader> Renderer::GetGfxLoader() const
{
    return m_GfxLoader;
}

void Renderer::PushLabel(vk::CommandBuffer cmdBuffer, std::string_view label) const
{
    cmdBuffer.beginDebugUtilsLabelEXT({ label.data() });
}

void Renderer::PopLabel(vk::CommandBuffer cmdBuffer) const
{
    cmdBuffer.endDebugUtilsLabelEXT();
}

void Renderer::BeginFrame()
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    m_CurrentFrameInFlight = m_Context->GetCurrentFrameIndex();
    m_Context->GetCommandPoolManager().ResetFrameCommands(m_Context->GetCurrentFrameIndex());
    vk::CommandBuffer cmdBuffer = m_Context->GetPrimaryCommandBuffer();
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(cmdBuffer, vk::ImageLayout::eGeneral);

    ProcessDirtyEffects(cmdBuffer);
    ProcessDirtyMaterials(cmdBuffer);
    CleanupDirtyEffects();

    if (m_LoadAsync == false)
        m_GfxLoader->Update();
    UpdateTextures(cmdBuffer);

    // Do we need this???
    auto viewport = m_Swapchain->GetViewport();
    cmdBuffer.setScissor(0, viewport.GetScissor());
    auto vp = viewport.GetViewport();
    vp.y += vp.height;
    vp.height *= -1;
    cmdBuffer.setViewport(0, vp);

    m_FrameData[m_CurrentFrameInFlight].DescriptorAllocator->Clear();
}

void Renderer::EndFrame()
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    m_LastUsedStaticMesh.Reset();
    m_LastUsedPipeline.Reset();
    auto currentImage = m_Swapchain->GetCurrentImage();
    vk::CommandBuffer cb = m_Context->GetPrimaryCommandBuffer();
    currentImage->TransitionLayout(cb, vk::ImageLayout::ePresentSrcKHR);

    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::SubmitInfo submitInfo = m_Swapchain->GetSubmitInfo(waitStages, m_Context->GetCurrentFrameIndex());
    FrameCommands commands = m_Context->GetCommandPoolManager().EndFrame(m_Context->GetCurrentFrameIndex());
    submitInfo.setCommandBuffers(commands.CommandBuffers);
    m_Context->SubmitToQueue(EQueueFamilyType::Graphics, submitInfo, commands.Fence);
}

void Renderer::PostFrame()
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    m_Context->DeferredNukeResources();
}

void Renderer::BeginScene(SafePtr<WorldRenderer> worldRenderer,
                          SafePtr<FrameGraph> frameGraph,
                          WorldData globalData,
                          SafePtr<class UniformBuffer> worldGlobalUniforms)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    m_CurrentWorldRenderer = worldRenderer;
    m_CurrentFrameGraph = frameGraph;
    uint32_t imageIndex = m_Context->GetCurrentFrameIndex();
    vk::CommandBuffer cmdBuffer = m_Context->GetPrimaryCommandBuffer();
    FrameData& frameData = m_FrameData[imageIndex];
    frameData.CurrentWorldDataUniforms = worldGlobalUniforms;
    globalData.BRDFLut = m_BRDFLut->GetBindlessTextureHandle();
    frameData.CurrentWorldData = globalData;
    frameData.CurrentWorldDataUniforms->CopyData(cmdBuffer, globalData);

    frameData.DescriptorSet = frameData.DescriptorAllocator->Allocate(frameData.DescriptorSetLayout);

    auto bufferInfo = worldGlobalUniforms->GetDescriptorInfo();
    vk::WriteDescriptorSet writeDescriptorSet = vk::WriteDescriptorSet{
            frameData.DescriptorSet,
            0,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &bufferInfo,
            nullptr
    };

    writeDescriptorSet.dstSet = frameData.DescriptorSet;
    writeDescriptorSet.dstBinding = 0;

    m_Context->GetDevice().updateDescriptorSets(writeDescriptorSet, nullptr);
}

void Renderer::BeginRenderPass(const Framebuffer& framebuffer) const
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    framebuffer.Bind(m_Context->GetPrimaryCommandBuffer());

}

void Renderer::EndRenderPass(const Framebuffer& framebuffer) const
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    framebuffer.Unbind(m_Context->GetPrimaryCommandBuffer());
}

void Renderer::Draw(vk::CommandBuffer cmdBuffer,
                    const SafePtr<StaticMesh>& mesh,
                    const SafePtr<StandaloneStorageBuffer>& transformBuffer,
                    PassID passId,
                    uint32_t offset, uint32_t subMeshIndex,
                    uint32_t instanceCount)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    auto& submesh = mesh->GetSubMeshes()[subMeshIndex];
    auto material = mesh->GetMaterial(submesh.MaterialIndex);
    auto pipeline = material->GetPipeline(passId, m_CurrentFrameGraph);
    auto effect = material->GetTechnique()->GetPassEffect(passId);
    if (effect == nullptr || pipeline == nullptr)
    {
        LNE_ERROR("Pipeline is null. No draw call issued.");
        return;
    }
    
    vk::Device device = m_Context->GetDevice();

    bool hasPipelineChanged = false;
    SafePtr descAllocator = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator;
    if (pipeline != m_LastUsedPipeline)
    {
        pipeline->Bind(cmdBuffer);
        m_LastUsedPipeline = pipeline;
        hasPipelineChanged = true;

        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0,
                                     { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet, transformBuffer->GetDescSet() }, {});
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 4,
                                     { m_Context->GetBindlessDescriptorSet() }, {});
    }
    if (hasPipelineChanged || mesh != m_LastUsedStaticMesh)
    {
        LNE_PROFILE_SCOPE_C("Set Geometry DescSet", PROFILING_COL)
            const Geometry& geometry = mesh->GetGeometry();
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 2,
                                     { geometry.GetDescSet() }, {});
        m_LastUsedStaticMesh = mesh;
    }

    if (hasPipelineChanged || m_LastUsedEffect != effect)
    {
        LNE_PROFILE_SCOPE_C("Set Effect DescSet", PROFILING_COL)
        auto matDescSet = effect->GetFrameDescriptorSet(m_CurrentFrameInFlight);
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline->GetLayout(), 3,
            { matDescSet },
            {}
        );
        m_LastUsedEffect = effect;
    }
    auto matSlot = material->GetMaterialPassSlot(passId);
    cmdBuffer.pushConstants<MaterialSlot>(pipeline->GetLayout(), matSlot.Stages, 0, { matSlot.Slot });
    cmdBuffer.draw(submesh.IndexCount, instanceCount, submesh.BaseIndex, offset);
}

void Renderer::DrawFullscreenQuad(vk::CommandBuffer cmdBuffer, SafePtr<Material> material, PassID passId)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    if (material->GetMaterialType() != ShaderDomain::ePostProcess)
    {
        LNE_ERROR("Material type not supported for fullscreen quad");
        return;
    }
    SafePtr technique = material->GetTechnique();
    SafePtr effect = technique->GetPassEffect(passId);
    SafePtr pipeline = material->GetPipeline(passId, m_CurrentFrameGraph);
    if (pipeline == nullptr)
    {
        LNE_ERROR("Pipeline is null");
        return;
    }
    vk::Device device = m_Context->GetDevice();
    const Geometry& geometry = m_Context->GetDefaultFullscreenQuad();
    bool hasPipelineChanged = false;
    if (pipeline != m_LastUsedPipeline)
    {
        pipeline->Bind(cmdBuffer);
        m_LastUsedPipeline = pipeline;
        hasPipelineChanged = true;

        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0,
                                     { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet }, {});
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 3,
                                     { m_Context->GetBindlessDescriptorSet() }, {});
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 1,
                                     { geometry.GetDescSet() }, {});
    }
    auto matSlot = material->GetMaterialPassSlot(passId);

    cmdBuffer.pushConstants<MaterialSlot>(pipeline->GetLayout(), matSlot.Stages, 0, { matSlot.Slot });
    if (m_LastUsedEffect != effect || m_LastUsedPipeline != pipeline)
    {
        auto matDescSet = effect->GetFrameDescriptorSet(m_CurrentFrameInFlight);
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline->GetLayout(), 2,
            { matDescSet },
            {}
        );
        m_LastUsedEffect = effect;
    }
    cmdBuffer.draw(geometry.GetIndexCount(), 1, 0, 0);
}

void Renderer::Dispatch(SafePtr<ComputeProgram> program, uint32_t x, uint32_t y, uint32_t z, bool async)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    vk::CommandBuffer cmdBuffer{};
    CommandPoolManager& cpManager = m_Context->GetCommandPoolManager();

    if (async == false)
        cmdBuffer = cpManager.BeginOrGetPrimaryFrameCommandBuffer(m_Context->GetCurrentFrameIndex());
    else
        cmdBuffer = cpManager.BeginOrGetSingleUseCommandBuffer(EQueueFamilyType::Compute);

    Dispatch(cmdBuffer, program, x, y, z);

    if (async)
        cpManager.EndSingleUseCommandBuffer(EQueueFamilyType::Compute);
}

void Renderer::Dispatch(vk::CommandBuffer cmdBuffer, SafePtr<class ComputeProgram> program, uint32_t x, uint32_t y, uint32_t z)
{
    PushLabel(cmdBuffer, std::format("Compute Dispatch {}", program->GetPipeline()->GetName()));
    auto pipeline = program->GetPipeline();
    pipeline->Bind(cmdBuffer);

    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline->GetLayout(), 0, { program->m_DescriptorSet, m_Context->GetBindlessDescriptorSet() }, {});

    cmdBuffer.dispatch(x, y, z);

    PopLabel(cmdBuffer);
}

void Renderer::Blit(vk::CommandBuffer cmdBuffer, SafePtr<Texture> src, SafePtr<Texture> dst)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    vk::ImageLayout srcLayout = src->GetLayout();
    vk::ImageLayout dstLayout = dst->GetLayout();
    src->TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferSrcOptimal);
    dst->TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferDstOptimal);

    auto srcExtent = src->GetDimensions();
    auto dstExtent = dst->GetDimensions();
    vk::ImageBlit blit{
        vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor,
            0,
            0,
            1
        },
        {
            vk::Offset3D{ 0, 0, 0 },
            vk::Offset3D{ (int)srcExtent.width, (int)srcExtent.height, 1 }
        },
        vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor,
            0,
            0,
            1
        },
        {
            vk::Offset3D{ 0, 0, 0 },
            vk::Offset3D{ (int)dstExtent.width, (int)dstExtent.height, 1 }
        }
    };
    
    cmdBuffer.blitImage(
        src->m_Allocation.Image, vk::ImageLayout::eTransferSrcOptimal,
        dst->m_Allocation.Image, vk::ImageLayout::eTransferDstOptimal,
        1, &blit, vk::Filter::eLinear
    );

    src->TransitionLayout(cmdBuffer, srcLayout);
    dst->TransitionLayout(cmdBuffer, dstLayout);
}

SafePtr<GfxPipeline> Renderer::CreateGraphicsPipeline(const GraphicsPipelineDesc& createInfo)
{
    SafePtr<GfxPipeline> pipeline;
    pipeline.Reset(lnnew GfxPipeline(m_Context, createInfo));
    return pipeline;
}

SafePtr<class StorageBuffer> Renderer::CreateGeometryBuffer(const void* data, size_t size)
{
    SafePtr<StorageBuffer> buffer;
    buffer.Reset(lnnew StorageBuffer(m_Context, (uint64_t)size, data));
    return buffer;
}

SafePtr<Texture> Renderer::CreateTexture(const std::string& fullPath, vk::Format format)
{
    return m_GfxLoader->CreateTexture(fullPath, format);
}

SafePtr<Texture> Renderer::CreateCubemapTexture(const std::vector<std::string>& faces)
{
    return m_GfxLoader->CreateCubemap(faces);
}

SafePtr<WorldEnvironment> Renderer::CreateEnvironmentMap(std::string_view pathToEnvMap, uint32_t dimensions)
{
    return m_GfxLoader->CreateEnvironmentMap(pathToEnvMap);
}

SafePtr<Shader> Renderer::CreateOrGetShader(const std::string& path)
{
    namespace fs = std::filesystem;

    if (path.empty() || std::filesystem::exists(path) == false)
    {
        LNE_ERROR("Shader path is empty");
        return {};
    }
    std::string_view assetsPath = ApplicationBase::GetAssetsPath();
    fs::path rel = fs::weakly_canonical(path).lexically_relative(fs::weakly_canonical(assetsPath));
    if (rel.empty() || *rel.begin() == "..")
    {
        LNE_ERROR("Effect path is not relative to assets path");
        return {};
    }
    std::lock_guard<std::mutex> lock(m_ShadersLibraryMutex);
    auto it = m_ShadersLibrary.find(rel.string());
    if (it != m_ShadersLibrary.end())
        return it->second;
    auto shader = SafePtr(lnnew Shader(m_Context, path));
    m_ShadersLibrary[rel.string()] = shader;
    return shader;
}

SafePtr<Effect> Renderer::CreateOrGetEffect(const std::string& path)
{
    namespace fs = std::filesystem;

    if (path.empty() || fs::exists(path) == false)
    {
        LNE_ERROR("Effect path is empty");
        return {};
    }
    std::string_view assetsPath = ApplicationBase::GetAssetsPath();
    fs::path rel = fs::weakly_canonical(path).lexically_relative(fs::weakly_canonical(assetsPath));
    if (rel.empty() || *rel.begin() == "..")
    {
        LNE_ERROR("Effect path is not relative to assets path");
        return {};
    }
    std::lock_guard<std::mutex> lock(m_EffectsLibraryMutex);
    auto it = m_EffectsLibrary.find(rel.string());
    if (it != m_EffectsLibrary.end())
        return it->second;
    auto effect = SafePtr(lnnew Effect(m_Context, path));
    m_EffectsLibrary[rel.string()] = effect;
    return effect;
}

SafePtr<GfxTechnique> Renderer::CreateOrGetTechnique(const GfxTechniqueDesc& techniqueDesc)
{
    if (techniqueDesc.Name.empty())
    {
        LNE_ERROR("Technique name is empty");
        return {};
    }
    std::lock_guard<std::mutex> lock(m_TechniquesLibraryMutex);
    auto it = m_TechniquesLibrary.find(techniqueDesc.Name);
    if (it != m_TechniquesLibrary.end())
        return it->second;
    auto technique = SafePtr(lnnew GfxTechnique(techniqueDesc));
    m_TechniquesLibrary[techniqueDesc.Name] = technique;
    return technique;
}

lne::SafePtr<lne::GfxTechnique> Renderer::GetTechnique(const std::string& name)
{
    return {};
}

void Renderer::AddTextureToUpdate(SafePtr<class Texture> texture)
{
    std::lock_guard<std::mutex> lock(m_TexturesToUpdateMutex);
    m_TexturesToUpdate.push_back(texture);
}

void Renderer::AddDirtyEffect(SafePtr<Effect> effect)
{
    std::lock_guard<std::mutex> lock(m_DirtyEffectsMutex);
    m_DirtyEffects.push_back(effect);
}

void Renderer::AddDirtyMaterial(SafePtr<Material> material)
{
    std::lock_guard<std::mutex> lock(m_DirtyMaterialsMutex);
    m_DirtyMaterials.push_back(material);
}

lne::SafePtr<class Texture> Renderer::GetBRDFLut() const
{
    return m_BRDFLut;
}

lne::SafePtr<lne::Texture> Renderer::GetDefaultTexture() const
{
    return m_Context->GetDefaultTexture();
}

lne::SafePtr<lne::Texture> Renderer::GetWhiteTexture() const
{
    return m_Context->GetWhiteTexture();
}

std::filesystem::path Renderer::GetShaderCachePath() const
{
    return std::filesystem::path(ApplicationBase::GetAssetsPath()) / "Engine" / "Shaders" / "Cache";
}

void Renderer::InitFrameData(uint32_t index)
{
    m_FrameData.emplace_back(
        SafePtr(lnnew DynamicDescriptorAllocator(m_Context.GetPtr(),
            {
                { vk::DescriptorType::eUniformBuffer, 1 },
                { vk::DescriptorType::eStorageBuffer, 4 }
            },
            "GlobalDescAlloc" + std::to_string(index), 32)),
        m_Context->CreateDescriptorSetLayout({
            vk::DescriptorSetLayoutBinding{
                0,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eAll
            }
            })
    );
}

void Renderer::UpdateTextures(vk::CommandBuffer cmdBuffer)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    std::lock_guard<std::mutex> lock(m_TexturesToUpdateMutex);
    if (m_TexturesToUpdate.empty())
        return;

    for (auto& texture : m_TexturesToUpdate)
    {
        texture->TransitionLayout(cmdBuffer, vk::ImageLayout::eTransferDstOptimal,
                m_Context->GetQueueFamilyIndex(EQueueFamilyType::Transfer), m_Context->GetQueueFamilyIndex(EQueueFamilyType::Graphics));

        if (texture->ShouldGenerateMips() == false)
        {
            texture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            continue;
        }

        texture->GenerateMipmaps(cmdBuffer);

        texture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    m_TexturesToUpdate.clear();
}

void Renderer::ProcessDirtyEffects(vk::CommandBuffer cmdBuffer)
{
    std::lock_guard<std::mutex> lock(m_DirtyEffectsMutex);
    if (m_DirtyEffects.empty())
        return;
    uint32_t currentFrameInFlight = m_Context->GetCurrentFrameIndex();
    for (size_t i = m_DirtyEffects.size(); i-- > 0; )
    {
        auto effect = m_DirtyEffects[i];
        effect->GrowBank(cmdBuffer, currentFrameInFlight);
    }
}

void Renderer::CleanupDirtyEffects()
{
    for (size_t i = m_DirtyEffects.size(); i-- > 0; )
    {
        auto effect = m_DirtyEffects[i];
        --effect->m_DirtyFrames; 
        if (effect->m_DirtyFrames == 0)
        {
            std::swap(m_DirtyEffects[i], m_DirtyEffects.back());
            m_DirtyEffects.pop_back();
        }
    }
}

void Renderer::ProcessDirtyMaterials(vk::CommandBuffer cmdBuffer)
{
    std::lock_guard<std::mutex> lock(m_DirtyMaterialsMutex);
    if (m_DirtyMaterials.empty())
        return;
    uint32_t currentFrameInFlight = m_Context->GetCurrentFrameIndex();
    for (size_t i = m_DirtyMaterials.size(); i-- > 0; )
    {
        auto material = m_DirtyMaterials[i];
        if (material->CopyPassDataToBuffers(cmdBuffer, currentFrameInFlight))
            --material->m_DirtyFrames;
        if (material->m_DirtyFrames == 0)
        {
            std::swap(m_DirtyMaterials[i], m_DirtyMaterials.back());
            m_DirtyMaterials.pop_back();
        }
    }
}

}
