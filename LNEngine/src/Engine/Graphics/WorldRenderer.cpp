#include "lnepch.h"
#include "FrameGraph/FrameGraph.h"
#include "DynamicDescriptorAllocator.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "Resources/Material.h"
#include "CommandPoolManager.h"
#include "WorldRenderer.h"
#include <Graphics/FrameGraph/RenderPass/IRenderPass.h>
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
    m_Transfroms.resize(maxFramesInFlight);
    // 2 MB of transform data per frame since a mat4 is 64 bytes. 1024 * 32 = 32k transforms
    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        m_TransformBuffers[i].Buffer.Reset(lnnew StandaloneStorageBuffer(gfxContext, sizeof(glm::mat4) * 1024 * 32, nullptr, StorageBufferType::eDynamic));
        m_TransformBuffers[i].Data = lnnew glm::mat4[1024*32];
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
    m_Transfroms[currentFrameIndex].clear();
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
        std::vector<SafePtr<IRenderPass>> staticMeshRenderPasses = m_FrameGraph->GetRenderPassesWithSignature(ComponentType<StaticMeshComponent>());
        std::vector<IDrawStaticMeshes*> drawStaticMeshesAdders;

        for (auto& renderPass : staticMeshRenderPasses)
        {
            IDrawStaticMeshes* drawStaticMeshesAdder = dynamic_cast<IDrawStaticMeshes*>(renderPass.GetPtr());
            if (drawStaticMeshesAdder)
                drawStaticMeshesAdders.push_back(drawStaticMeshesAdder);
        }
        auto& currTransforms = m_Transfroms[currentFrameIndex];
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
    auto renderTask = [this, totalSizeBytes]()
        {
            LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
            auto& renderer = ApplicationBase::GetRenderer();
            vk::CommandBuffer cmdBuffer = renderer.GetGfxContext()->GetPrimaryCommandBuffer();
            m_TransformBuffers[renderer.GetCurrentFrameIndex()].Buffer->CopyData(
                cmdBuffer,
                m_TransformBuffers[renderer.GetCurrentFrameIndex()].Data, totalSizeBytes, 0);
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
