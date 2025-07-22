#include "enkiTS/src/TaskScheduler.h"
#include "Renderer.h"
#include "Core/Utils/Log.h"
#include "Engine/Core/Window.h"
#include "GfxContext.h"
#include "CommandPoolManager.h"
#include "Texture.h"
#include "Framebuffer.h"
#include "Graphics/Pipeline.h"
#include "Core/Utils/Defines.h"
#include "Core/Utils/Profiling.h"
#include "DynamicDescriptorAllocator.h"
#include "Mesh.h"
#include "StorageBuffer.h"
#include "Scene/Components.h"
#include "Material.h"
#include "Resources/GfxLoader.h"
#include "Mesh.h"

// TODO: move this to a resource manager
#include <stb/stb_image.h>
#include "WorldEnvironment.h"

namespace lne
{
#define PROFILING_COL 0xFF5B53
void Renderer::Init(std::unique_ptr<Window>& window, std::shared_ptr<enki::TaskScheduler> taskScheduler)
{
    m_Context = window->GetGfxContext();
    m_Swapchain = window->GetSwapchain();
    m_TaskScheduler = taskScheduler;
    m_GfxLoader = lnnew GfxLoader();
    m_GfxLoader->Init(this, m_Context, m_TaskScheduler);
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
    m_LastUsedPipeline.Reset();
    m_LastUsedStaticMesh.Reset();
    for (auto& frameData : m_FrameData)
    {
        frameData.GlobalUniforms.Nuke();
        frameData.DescriptorAllocator.Reset();
        m_Context->GetDevice().destroyDescriptorPool(frameData.GlobalDescriptorPool);
        m_Context->GetDevice().destroyDescriptorSetLayout(frameData.DescriptorSetLayout);
    }
    m_FrameData.clear();
    m_Context.Reset();
    m_Swapchain.Reset();
    m_GfxLoader.Reset();
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
    uint32_t imageIndex = m_Context->GetCurrentFrameIndex();
    m_Context->GetCommandPoolManager().ResetFrameCommands(m_Context->GetCurrentFrameIndex());
    vk::CommandBuffer cmdBuffer = m_Context->GetPrimaryCommandBuffer();
    auto currentImage = m_Swapchain->GetCurrentImage();
    currentImage->TransitionLayout(cmdBuffer, vk::ImageLayout::eGeneral);

    UpdateTextures(cmdBuffer);

    // Do we need this???
    auto viewport = m_Swapchain->GetViewport();
    cmdBuffer.setScissor(0, viewport.GetScissor());
    auto vp = viewport.GetViewport();
    vp.y += vp.height;
    vp.height *= -1;
    cmdBuffer.setViewport(0, vp);

    m_FrameData[imageIndex].DescriptorAllocator->Clear();
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
    vk::SubmitInfo submitInfo = m_Swapchain->GetSubmitInfo(waitStages);
    FrameCommands commands = m_Context->GetCommandPoolManager().EndFrame(m_Context->GetCurrentFrameIndex());
    submitInfo.setCommandBuffers(commands.CommandBuffers);
    m_Context->SubmitToQueue(EQueueFamilyType::Graphics, submitInfo, commands.Fence);
}

void Renderer::PostFrame()
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    m_Context->DeferredNukeResources();
}

void Renderer::BeginScene(const TransformComponent& cameraTransform, const CameraComponent& camera, const glm::vec3& sunDirection, float ambientLight)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    uint32_t imageIndex = m_Context->GetCurrentFrameIndex();
    vk::CommandBuffer cmdBuffer = m_Context->GetPrimaryCommandBuffer();

    GlobalUniforms uniforms = {
        .ViewProj = camera.GetViewProj(),
        .View = camera.View,
        .Proj = camera.Proj,
        .CameraPosition = cameraTransform.Position,
        .SunDirection = sunDirection,
        .AmbientLight = ambientLight
    };

    m_FrameData[imageIndex].GlobalUniforms.CopyData(cmdBuffer, uniforms);
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

void Renderer::Draw(SafePtr<Material> material, struct Geometry& geometry, TransformComponent& objTransform)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    auto pipeline = material->GetPipeline();
    vk::CommandBuffer cmdBuffer = m_Context->GetPrimaryCommandBuffer();
    pipeline->Bind(cmdBuffer);
    
    // Create & update geometry descriptor set
    auto geometryDescSetLayout = pipeline->GetDescriptorSetLayouts()[1];
    vk::DescriptorSet geometryDescSet = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(geometryDescSetLayout);

    auto vertexInfo = geometry.VertexGPUBuffer->GetDescriptorInfo();
    auto indexInfo = geometry.IndexGPUBuffer->GetDescriptorInfo();
    std::vector<vk::WriteDescriptorSet> writeGeoDescriptorSets;
    writeGeoDescriptorSets.emplace_back(vk::WriteDescriptorSet{
        geometryDescSet,
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &vertexInfo,
        nullptr
    });
    writeGeoDescriptorSets.emplace_back(vk::WriteDescriptorSet{
        geometryDescSet,
        1,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &indexInfo,
        nullptr
    });

    m_Context->GetDevice().updateDescriptorSets(writeGeoDescriptorSets, nullptr);

    // Create & update object descriptor set
    objTransform.UniformBuffers->CopyData(cmdBuffer, objTransform.GetModelMatrix());
    auto objDescSetLayout = pipeline->GetDescriptorSetLayouts()[2];
    vk::DescriptorSet objDescSet = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(objDescSetLayout);

    auto objInfo = objTransform.UniformBuffers->GetCurrentBuffer().GetDescriptorInfo();
    vk::WriteDescriptorSet writeObjDescriptorSet = vk::WriteDescriptorSet{
        objDescSet,
        0,
        0,
        1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        &objInfo,
        nullptr
    };
    m_Context->GetDevice().updateDescriptorSets(writeObjDescriptorSet, nullptr);

    std::vector<vk::WriteDescriptorSet> matWriteDescriptorSets;
    std::vector<vk::DescriptorBufferInfo> matUbInfo;
    matUbInfo.reserve(material->m_UniformBuffers.size());
    vk::DescriptorSet matDescSet = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[3]);
    for (const auto& [binding, ub] : material->m_UniformBuffers)
    {
        matUbInfo.emplace_back(ub.GetDescriptorInfo());
        matWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            matDescSet,
            binding,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &matUbInfo.back(),
            nullptr
        });
    }
    m_Context->GetDevice().updateDescriptorSets(matWriteDescriptorSets, nullptr);

    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0, { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet, geometryDescSet, objDescSet, matDescSet, m_Context->GetBindlessDescriptorSet() }, {});
    cmdBuffer.draw(geometry.IndexCount, 1, 0, 0);
}

void Renderer::Draw(SafePtr<StaticMesh> mesh, TransformComponent& objTransform)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL);
    vk::CommandBuffer cmdBuffer = m_Context->GetPrimaryCommandBuffer();
    auto pipeline = mesh->GetPipeline();
    auto& geometry = mesh->GetGeometry();
    pipeline->Bind(cmdBuffer);

    auto& submeshes = mesh->GetSubMeshes();
    for (const auto& submesh : submeshes)
    {
        auto material = mesh->GetMaterial(submesh.MaterialIndex);


        // Create & update geometry descriptor set
        auto geometryDescSetLayout = pipeline->GetDescriptorSetLayouts()[1];
        vk::DescriptorSet geometryDescSet = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(geometryDescSetLayout);

        auto vertexInfo = geometry.VertexGPUBuffer->GetDescriptorInfo();
        auto indexInfo = geometry.IndexGPUBuffer->GetDescriptorInfo();
        std::vector<vk::WriteDescriptorSet> writeGeoDescriptorSets;
        writeGeoDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            geometryDescSet,
            0,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &vertexInfo,
            nullptr
            });
        writeGeoDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            geometryDescSet,
            1,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &indexInfo,
            nullptr
            });

        m_Context->GetDevice().updateDescriptorSets(writeGeoDescriptorSets, nullptr);

        // Create & update object descriptor set
        objTransform.UniformBuffers->CopyData(cmdBuffer, objTransform.GetModelMatrix());
        auto objDescSetLayout = pipeline->GetDescriptorSetLayouts()[2];
        vk::DescriptorSet objDescSet = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(objDescSetLayout);

        auto objInfo = objTransform.UniformBuffers->GetCurrentBuffer().GetDescriptorInfo();
        vk::WriteDescriptorSet writeObjDescriptorSet = vk::WriteDescriptorSet{
            objDescSet,
            0,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &objInfo,
            nullptr
        };
        m_Context->GetDevice().updateDescriptorSets(writeObjDescriptorSet, nullptr);

        std::vector<vk::WriteDescriptorSet> matWriteDescriptorSets;
        std::vector<vk::DescriptorBufferInfo> matUbInfo;
        matUbInfo.reserve(material->m_UniformBuffers.size());
        vk::DescriptorSet matDescSet = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[3]);
        for (const auto& [binding, ub] : material->m_UniformBuffers)
        {
            matUbInfo.emplace_back(ub.GetDescriptorInfo());
            matWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
                matDescSet,
                binding,
                0,
                1,
                vk::DescriptorType::eUniformBuffer,
                nullptr,
                &matUbInfo.back(),
                nullptr
                });
        }
        m_Context->GetDevice().updateDescriptorSets(matWriteDescriptorSets, nullptr);

        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0, { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet, geometryDescSet, objDescSet, matDescSet, m_Context->GetBindlessDescriptorSet() }, {});
        cmdBuffer.draw(submesh.IndexCount, 1, submesh.BaseIndex, 0);
    }
}

void Renderer::Draw(vk::CommandBuffer cmdBuffer, const SafePtr<lne::StaticMesh>& mesh, const SafePtr<lne::StorageBuffer>& transformBuffer,
    uint32_t offset, uint32_t subMeshIndex, uint32_t instanceCount)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    auto& submesh = mesh->GetSubMeshes()[subMeshIndex];
    auto material = mesh->GetMaterial(submesh.MaterialIndex);
    auto pipeline = material->GetPipeline();
    if (pipeline == nullptr)
    {
        LNE_ERROR("Pipeline is null");
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

        auto transformDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[1]);

        vk::DescriptorBufferInfo transformInfo = transformBuffer->GetDescriptorInfo();
        vk::WriteDescriptorSet writeTransformDescriptorSet = vk::WriteDescriptorSet{
            transformDescSet,
            0,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &transformInfo,
            nullptr
        };
        device.updateDescriptorSets(writeTransformDescriptorSet, nullptr);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0,
        { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet, transformDescSet }, {});
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 4,
        { m_Context->GetBindlessDescriptorSet() }, {});
    }
    if (hasPipelineChanged || mesh != m_LastUsedStaticMesh)
    {
        LNE_PROFILE_SCOPE_C("Set Geometry DescSet", PROFILING_COL)
        const Geometry& geometry = mesh->GetGeometry();
        vk::DescriptorSet geometryDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[2]);
        
        vk::DescriptorBufferInfo vertexInfo = geometry.VertexGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo indexInfo = geometry.IndexGPUBuffer->GetDescriptorInfo();
        
        std::vector<vk::WriteDescriptorSet> writeGeoDescriptorSets;
        writeGeoDescriptorSets.emplace_back(
            geometryDescSet, 0, 0, 1,
            vk::DescriptorType::eStorageBuffer, nullptr, &vertexInfo, nullptr
        );
        writeGeoDescriptorSets.emplace_back(
            geometryDescSet, 1, 0, 1,
            vk::DescriptorType::eStorageBuffer, nullptr, &indexInfo, nullptr
        );
        
        device.updateDescriptorSets(writeGeoDescriptorSets, nullptr);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 2,
        { geometryDescSet }, {});
        m_LastUsedStaticMesh = mesh;
    }

    {
        LNE_PROFILE_SCOPE_C("Set Material DescSet", PROFILING_COL)
            vk::DescriptorSet matDescSet{};
        uint32_t currFrameInd = GetCurrentFrameIndex();
        if (material->m_CurrentFrameInFlight != currFrameInd)
        {
            std::vector<vk::WriteDescriptorSet> matWriteDescriptorSets;
            std::vector<vk::DescriptorBufferInfo> matUbInfo;
            matUbInfo.reserve(material->m_UniformBuffers.size());
            matDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[3]);
            for (const auto& [binding, ub] : material->m_UniformBuffers)
            {
                matUbInfo.emplace_back(ub.GetDescriptorInfo());
                matWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
                    matDescSet,
                    binding,
                    0,
                    1,
                    vk::DescriptorType::eUniformBuffer,
                    nullptr,
                    &matUbInfo.back(),
                    nullptr
                    });
            }

            m_Context->GetDevice().updateDescriptorSets(matWriteDescriptorSets, nullptr);
            material->m_CurrentFrameInFlight = currFrameInd;
            material->m_DescSets[currFrameInd] = matDescSet;
        }
        else
        {
            matDescSet = material->m_DescSets[currFrameInd];
        }
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline->GetLayout(), 3,
            { matDescSet },
            {}
        );
    }
    cmdBuffer.draw(submesh.IndexCount, instanceCount, submesh.BaseIndex, offset);
}

void Renderer::Draw(vk::CommandBuffer cmdBuffer, const SafePtr<lne::StaticMesh>& mesh, const SafePtr<lne::StorageBuffer>& transformBuffer, SafePtr<Material> overrideMaterial, uint32_t offset, uint32_t subMeshIndex, uint32_t instanceCount)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    auto& submesh = mesh->GetSubMeshes()[subMeshIndex];
    const auto& material = overrideMaterial;
    auto pipeline = material->GetPipeline();
    if (pipeline == nullptr)
    {
        LNE_ERROR("Pipeline is null");
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

        auto transformDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[1]);

        vk::DescriptorBufferInfo transformInfo = transformBuffer->GetDescriptorInfo();
        vk::WriteDescriptorSet writeTransformDescriptorSet = vk::WriteDescriptorSet{
            transformDescSet,
            0,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &transformInfo,
            nullptr
        };
        device.updateDescriptorSets(writeTransformDescriptorSet, nullptr);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0,
            { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet, transformDescSet }, {});
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 4,
            { m_Context->GetBindlessDescriptorSet() }, {});
    }
    if (hasPipelineChanged || mesh != m_LastUsedStaticMesh)
    {
        LNE_PROFILE_SCOPE_C("Set Geo DescSet", PROFILING_COL)
        const Geometry& geometry = mesh->GetGeometry();
        vk::DescriptorSet geometryDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[2]);

        vk::DescriptorBufferInfo vertexInfo = geometry.VertexGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo indexInfo = geometry.IndexGPUBuffer->GetDescriptorInfo();

        std::vector<vk::WriteDescriptorSet> writeGeoDescriptorSets;
        writeGeoDescriptorSets.emplace_back(
            geometryDescSet, 0, 0, 1,
            vk::DescriptorType::eStorageBuffer, nullptr, &vertexInfo, nullptr
        );
        writeGeoDescriptorSets.emplace_back(
            geometryDescSet, 1, 0, 1,
            vk::DescriptorType::eStorageBuffer, nullptr, &indexInfo, nullptr
        );

        device.updateDescriptorSets(writeGeoDescriptorSets, nullptr);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 2,
            { geometryDescSet }, {});
        m_LastUsedStaticMesh = mesh;
    }

    {
        LNE_PROFILE_SCOPE_C("Set Material DescSet", PROFILING_COL)
        vk::DescriptorSet matDescSet{};
        uint32_t currFrameInd = GetCurrentFrameIndex();
        if (material->m_CurrentFrameInFlight != currFrameInd)
        {
            std::vector<vk::WriteDescriptorSet> matWriteDescriptorSets;
            std::vector<vk::DescriptorBufferInfo> matUbInfo;
            matUbInfo.reserve(material->m_UniformBuffers.size());
            matDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[3]);
            for (const auto& [binding, ub] : material->m_UniformBuffers)
            {
                matUbInfo.emplace_back(ub.GetDescriptorInfo());
                matWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
                    matDescSet,
                    binding,
                    0,
                    1,
                    vk::DescriptorType::eUniformBuffer,
                    nullptr,
                    &matUbInfo.back(),
                    nullptr
                    });
            }

            m_Context->GetDevice().updateDescriptorSets(matWriteDescriptorSets, nullptr);
            material->m_CurrentFrameInFlight = currFrameInd;
            material->m_DescSets[currFrameInd] = matDescSet;
        }
        else
        {
            matDescSet = material->m_DescSets[currFrameInd];
        }
        cmdBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline->GetLayout(), 3,
            { matDescSet },
            {}
        );
    }
    cmdBuffer.draw(submesh.IndexCount, instanceCount, submesh.BaseIndex, offset);
}

void Renderer::DrawFullscreenQuad(vk::CommandBuffer cmdBuffer, const SafePtr<class Material>& material)
{
    LNE_PROFILE_FUNCTION_C(PROFILING_COL)
    if (material->GetMaterialType() != MaterialType::ePostProcess)
    {
        LNE_ERROR("Material type not supported for fullscreen quad");
        return;
    }
    const SafePtr<GfxPipeline>& pipeline = material->GetPipeline();
    if (pipeline == nullptr)
    {
        LNE_ERROR("Pipeline is null"); return;
    }
    vk::Device device = m_Context->GetDevice();
    const Geometry& geometry = m_Context->GetDefaultFullscreenQuad();

    bool hasPipelineChanged = false;
    SafePtr descAllocator = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator;
    
    if (pipeline != m_LastUsedPipeline)
    {
        pipeline->Bind(cmdBuffer);
        m_LastUsedPipeline = pipeline;
        hasPipelineChanged = true;

        //// BIND DESCRIPTOR SETS 0 AND 3 ////////////////////
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0,
            { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet }, {});
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 3,
            { m_Context->GetBindlessDescriptorSet() }, {});
    }
    if (hasPipelineChanged)
    {
        
        //// BIND DESCRIPTOR SETS 1 ////////////////////
        auto geometryDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[1]);
        
        vk::DescriptorBufferInfo vertexInfo = geometry.VertexGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo indexInfo = geometry.IndexGPUBuffer->GetDescriptorInfo();

        std::vector<vk::WriteDescriptorSet> writeGeoDescriptorSets = {};
        writeGeoDescriptorSets.reserve(2);
        writeGeoDescriptorSets.emplace_back(
            geometryDescSet, 0, 0, 1,
            vk::DescriptorType::eStorageBuffer, nullptr, &vertexInfo, nullptr
        );
        writeGeoDescriptorSets.emplace_back(
            geometryDescSet, 1, 0, 1,
            vk::DescriptorType::eStorageBuffer, nullptr, &indexInfo, nullptr
        );

        device.updateDescriptorSets(writeGeoDescriptorSets, nullptr);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 1,
            { geometryDescSet }, {});
    }

    std::vector<vk::WriteDescriptorSet> matWriteDescriptorSets;
    std::vector<vk::DescriptorBufferInfo> matUbInfo;
    matUbInfo.reserve(material->m_UniformBuffers.size());
    
    vk::DescriptorSet matDescSet = descAllocator->Allocate(pipeline->GetDescriptorSetLayouts()[2]);
    for (const auto& [binding, ub] : material->m_UniformBuffers)
    {
        matUbInfo.emplace_back(ub.GetDescriptorInfo());
        matWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            matDescSet,
            binding,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &matUbInfo.back(),
            nullptr
            });
    }
    m_Context->GetDevice().updateDescriptorSets(matWriteDescriptorSets, nullptr);
    cmdBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline->GetLayout(), 2,
        { matDescSet },
        {}
    );
    cmdBuffer.draw(geometry.IndexCount, 1, 0, 0);
}

// TODO: change to one DispatchAsync function and one Dispatch not async function
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
    PushLabel(cmdBuffer, std::format("Compute Dispatch"));
    auto pipeline = program->GetPipeline();
    pipeline->Bind(cmdBuffer);
    auto descSetAlloc = m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorAllocator;

    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline->GetLayout(), 0, { m_FrameData[m_Context->GetCurrentFrameIndex()].DescriptorSet }, {});

    std::vector<vk::WriteDescriptorSet> progWriteDescriptorSets;
    std::vector<vk::DescriptorBufferInfo> progUbInfo;
    progUbInfo.reserve(program->m_UniformBuffers.size());
    vk::DescriptorSet progDescSet = descSetAlloc->Allocate(pipeline->GetDescriptorSetLayouts()[1]);
    for (const auto& [binding, ub] : program->m_UniformBuffers)
    {
        progUbInfo.emplace_back(ub.GetDescriptorInfo());
        progWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            progDescSet,
            binding,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &progUbInfo.back(),
            nullptr
            });
    }
    m_Context->GetDevice().updateDescriptorSets(progWriteDescriptorSets, nullptr);

    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline->GetLayout(), 1, { progDescSet, m_Context->GetBindlessDescriptorSet() }, {});

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

SafePtr<UniformBufferManager> Renderer::RegisterObject()
{
    SafePtr<UniformBufferManager> uboManager;
    uboManager.Reset(lnnew UniformBufferManager(m_Context, sizeof(glm::mat4)));
    return uboManager;
}

void Renderer::AddTextureToUpdate(SafePtr<class Texture> texture)
{
    std::lock_guard<std::mutex> lock(m_TexturesToUpdateMutex);
    m_TexturesToUpdate.push_back(texture);
}

void Renderer::InitFrameData(uint32_t index)
{
    m_FrameData.emplace_back(
            UniformBuffer(m_Context, sizeof(GlobalUniforms)),
            SafePtr(lnnew DynamicDescriptorAllocator(m_Context, 
                { 
                    { vk::DescriptorType::eUniformBuffer, 1024 },
                    { vk::DescriptorType::eStorageBuffer, 1024 }
                }, 
                "GlobalDescAlloc" + std::to_string(index), 1)),
            m_Context->CreateDescriptorSetLayout({
                vk::DescriptorSetLayoutBinding{
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    1,
                    vk::ShaderStageFlagBits::eAll
                }
            })
        );

    std::array<vk::DescriptorPoolSize, 1> poolSizes = {
        vk::DescriptorPoolSize{
            vk::DescriptorType::eUniformBuffer,
            8
        },
    };
    vk::Device device = m_Context->GetDevice();
    m_FrameData[index].GlobalDescriptorPool = device.createDescriptorPool(
        vk::DescriptorPoolCreateInfo{
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            8, poolSizes
        },
        nullptr
    );

    m_FrameData[index].DescriptorSet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            m_FrameData[index].GlobalDescriptorPool,
            1, &m_FrameData[index].DescriptorSetLayout
        }
    ).front();

    auto bufferInfo = m_FrameData[index].GlobalUniforms.GetDescriptorInfo();
    m_Context->SetVkObjectName(m_FrameData[index].GlobalDescriptorPool, "GlobalDescriptorPool " + std::to_string(index));
    m_Context->SetVkObjectName(m_FrameData[index].DescriptorSet, "GlobalDescriptorSet " + std::to_string(index));
    vk::WriteDescriptorSet writeDescriptorSet = vk::WriteDescriptorSet{
            m_FrameData[index].DescriptorSet,
            0,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &bufferInfo,
            nullptr
    };

    writeDescriptorSet.dstSet = m_FrameData[index].DescriptorSet;
    writeDescriptorSet.dstBinding = 0;

    device.updateDescriptorSets(writeDescriptorSet, nullptr);
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

}
