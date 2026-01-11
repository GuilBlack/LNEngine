#include "lnepch.h"
#include "FrameGraph/FrameGraph.h"
#include "DynamicDescriptorAllocator.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "Resources/Material.h"
#include "CommandPoolManager.h"
#include "WorldRenderer.h"
#include <Graphics/FrameGraph/RenderPass/RenderPass.h>
#include "ECS/EntityRegistry.h"
#include "Scene/Components.h"
#include "Resources/Mesh.h"
#include "Core/Utils/Profiling.h"
#include <Scene/Entity.h>


namespace lne
{
WorldRenderer::WorldRenderer(const SafePtr<FrameGraph>& frameGraph)
    : m_FrameGraph(frameGraph)
{
    SafePtr<GfxContext> gfxContext = ApplicationBase::GetRenderer().GetGfxContext();
    uint32_t maxFramesInFlight = gfxContext->GetMaxFramesInFlight();
    m_WorldGlobalUniforms.reserve(maxFramesInFlight);
    
    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        m_WorldGlobalUniforms.push_back(lnnew UniformBuffer(gfxContext, sizeof(WorldData)));
    }

    m_TransformBuffers.resize(maxFramesInFlight);
    m_Transforms.resize(maxFramesInFlight);

    m_LightBuffersGPU.resize(maxFramesInFlight);
    m_LightsCPU.resize(maxFramesInFlight);
    m_NumLights.resize(maxFramesInFlight, 0);
    uint32_t initialNumLights = 512;

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        m_TransformBuffers[i].Buffer.Reset(lnnew StandaloneStorageBuffer(gfxContext, sizeof(glm::mat4) * 1024 * 32, nullptr, StorageBufferType::eDynamic));
        m_TransformBuffers[i].Data = lnnew glm::mat4[1024*32];

        m_LightBuffersGPU[i] = lnnew StandaloneStorageBuffer(gfxContext, 4 + sizeof(LightGPUData) * initialNumLights, nullptr, StorageBufferType::eDynamic);
        m_LightsCPU[i].resize(initialNumLights);
    }
}

WorldRenderer::~WorldRenderer()
{
    for (uint32_t i = 0; i < m_TransformBuffers.size(); ++i)
    {
        m_TransformBuffers[i].Buffer.Reset();
        delete[] m_TransformBuffers[i].Data;
    }
}

void WorldRenderer::SetEnvironmentMap(std::string_view pathToEnvMap)
{
    m_Environment = ApplicationBase::GetRenderer().CreateEnvironmentMap(pathToEnvMap);
}

void WorldRenderer::BeginScene(Entity& cameraEntity)
{
    LNE_PROFILE_FUNCTION()
    auto& nodes = m_FrameGraph->GetNodes();
    for (auto& nodeHandle : nodes)
    {
        FrameGraphNode* node = m_FrameGraph->GetNode(nodeHandle);
        node->RenderPass->BeginFrame();
    }
    uint32_t currentFrameIndex = ApplicationBase::GetRenderer().GetCurrentFrameIndexOnMainThread();
    m_Transforms[currentFrameIndex].clear();
    CameraComponent& cameraComponent = cameraEntity.GetComponent<CameraComponent>();
    m_GlobalData = WorldData{
        .ViewProj = cameraComponent.GetViewProj(),
        .View = cameraComponent.View,
        .Proj = cameraComponent.Proj,
        .CameraPosition = cameraEntity.GetComponent<TransformComponent>().Position,
        .SunDirection = m_Environment->SunDirection,
        .AmbientLight = m_Environment->AmbientLight,
        .IrradianceMap = m_Environment->IrradianceTexture->GetBindlessTextureHandle(),
        .PrefilteredMap = m_Environment->PrefilteredTexture->GetBindlessTextureHandle()
    };
    ApplicationBase::GetRenderer().BeginScene(this, m_FrameGraph, m_GlobalData, m_WorldGlobalUniforms[currentFrameIndex]);
}

void WorldRenderer::Render(EntityRegistry& registry)
{
    LNE_PROFILE_FUNCTION()
    auto& renderer = ApplicationBase::GetRenderer();
    uint32_t currentFrameIndex = renderer.GetCurrentFrameIndexOnMainThread();
    uint32_t totalSizeBytes;
    auto staticMeshView = registry.GetView<TransformComponent, StaticMeshComponent>();
    {
        LNE_PROFILE_SCOPE("Update Transform Buffer")
        std::vector<SafePtr<RenderPass>> staticMeshRenderPasses = m_FrameGraph->GetRenderPassesWithSignature(ComponentType<StaticMeshComponent>());
        std::vector<IDrawStaticMeshes*> drawStaticMeshesAdders;

        for (auto& renderPass : staticMeshRenderPasses)
        {
            IDrawStaticMeshes* drawStaticMeshesAdder = dynamic_cast<IDrawStaticMeshes*>(renderPass.GetPtr());
            if (drawStaticMeshesAdder)
                drawStaticMeshesAdders.push_back(drawStaticMeshesAdder);
        }
        auto& currTransforms = m_Transforms[currentFrameIndex];
        for (auto& index : staticMeshView)
        {
            auto [transform, staticMesh] = staticMeshView.Get(index);

            if (!staticMesh.Mesh)
                continue;
            // TODO: will probably insert frustum culling here
            auto& subMeshes = staticMesh.Mesh->GetSubMeshes();
            for (uint32_t i = 0; i < subMeshes.size(); ++i)
            {
                const SubMesh& subMesh = subMeshes[i];

                glm::mat4 model = transform.GetModelMatrix() * subMesh.WorldTransform;
                StaticMeshHash hash{ (uint64_t)staticMesh.Mesh.GetPtr(), i };
                auto& submeshTransformArray = currTransforms[hash];
                submeshTransformArray.Mesh = staticMesh.Mesh;
                submeshTransformArray.Transforms.emplace_back(model);
            }
        }

        for (auto& [hash, array] : currTransforms)
        {
            for (auto& drawStaticMeshesAdder : drawStaticMeshesAdders)
                drawStaticMeshesAdder->AddStaticMeshDrawCommand(hash, array.Mesh, hash.SubMeshIndex);
        }

        uint32_t offset = 0;
        for (auto& [hash, subMeshArray] : currTransforms)
        {
            uint32_t size = (uint32_t)subMeshArray.Transforms.size();
            if (size == 0)
                continue;
            subMeshArray.Offset = offset;
            // copy submesh transforms to the transform buffer
            void* dst = m_TransformBuffers[currentFrameIndex].Data + offset;
            std::memcpy(dst, subMeshArray.Transforms.data(), size * sizeof(glm::mat4));
            offset += size;
        }
        totalSizeBytes = offset * sizeof(glm::mat4);
    }

    {
        LNE_PROFILE_SCOPE("Update Light Buffer")
        auto lightView = registry.GetView<TransformComponent, LightComponent>();
        uint32_t& numLights = m_NumLights[currentFrameIndex];
        auto& lightsCPU = m_LightsCPU[currentFrameIndex];
        numLights = lightView.TotalSize();
        if (numLights > lightsCPU.capacity())
            lightsCPU.resize(numLights);

        for (auto& index : lightView)
        {
            auto [transform, light] = lightView.Get(index);
            lightsCPU[index.ComposedIndex] = LightGPUData{
                light.Type,
                transform.Position,
                transform.GetForward(),
                light.Color,
                light.Intensity,
                light.Range,
                light.Falloff,
                light.SpotAngle
            };
        }
    }

    // the this should be safe since it's more of a ref that we give to the
    // render thread which should be nuked before the end of the app.
    // so, no worries (I think)
    auto renderTask = [this, totalSizeBytes]()
        {
            LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
            auto& renderer = ApplicationBase::GetRenderer();
            vk::CommandBuffer cmdBuffer = renderer.GetGfxContext()->GetPrimaryCommandBuffer();
            uint32_t currentFrameIndex = renderer.GetCurrentFrameIndex();

            m_TransformBuffers[currentFrameIndex].Buffer->CopyData(
                cmdBuffer,
                m_TransformBuffers[currentFrameIndex].Data, totalSizeBytes, 0);

            uint32_t numLights = m_NumLights[currentFrameIndex];
            if (m_LightBuffersGPU[currentFrameIndex]->GetSize() < numLights * sizeof(LightGPUData))
            {
                m_LightBuffersGPU[currentFrameIndex]->Grow(
                    cmdBuffer,
                    4 + numLights * sizeof(LightGPUData) * 2, false);
            }

            // TODO: Mayby reduce this to a since copy instead of two
            // (dunno if it's really necessary tho. need to test)
            m_LightBuffersGPU[currentFrameIndex]->CopyData(
                cmdBuffer, &numLights,
                sizeof(uint32_t), 0);
            m_LightBuffersGPU[currentFrameIndex]->CopyData(
                cmdBuffer, m_LightsCPU[currentFrameIndex].data(),
                numLights * sizeof(LightGPUData), 4);

            renderer.PushLabel(cmdBuffer, "Frame");
            m_FrameGraph->Execute(cmdBuffer, this);
            renderer.PopLabel(cmdBuffer);
        };
    if (renderer.IsAsync())
        renderer.AddRenderTask(renderTask);
    else
        renderTask();
}

void WorldRenderer::EndFrame()
{
    LNE_PROFILE_FUNCTION()
    auto& nodes = m_FrameGraph->GetNodes();
    for (auto& nodeHandle : nodes)
    {
        FrameGraphNode* node = m_FrameGraph->GetNode(nodeHandle);
        node->RenderPass->EndFrame();
    }
}
}
