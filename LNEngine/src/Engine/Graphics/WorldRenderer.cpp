#include "lnepch.h"
#include "FrameGraph/FrameGraph.h"
#include "DynamicDescriptorAllocator.h"
#include "Core/ApplicationBase.h"
#include "Renderer.h"
#include "Material.h"
#include "CommandPoolManager.h"
#include "WorldRenderer.h"
#include <Graphics/FrameGraph/RenderPass/IRenderPass.h>
#include "ECS/EntityRegistry.h"
#include "Scene/Components.h"
#include "Mesh.h"
#include "Core/Utils/Profiling.h"
#include "Graphics/WorldEnvironment.h"


namespace lne
{
WorldRenderer::WorldRenderer(const SafePtr<FrameGraph>& frameGraph)
    : m_FrameGraph(frameGraph)
{
    SafePtr<GfxContext> gfxContext = ApplicationBase::GetWindow().GetGfxContext();
    uint32_t maxFramesInFlight = gfxContext->GetMaxFramesInFlight();

    m_TransformBuffers.resize(maxFramesInFlight);
    // 2 MB of transform data per frame since a mat4 is 64 bytes. 1024 * 32 = 32k transforms
    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        m_TransformBuffers[i].Buffer.Reset(lnnew StorageBuffer(gfxContext, sizeof(glm::mat4) * 1024 * 32, nullptr, StorageBufferType::eDynamic));
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

void WorldRenderer::BeginFrame()
{
    LNE_PROFILE_FUNCTION()
    auto& nodes = m_FrameGraph->GetNodes();
    for (auto& nodeHandle : nodes)
    {
        FrameGraphNode* node = m_FrameGraph->GetNode(nodeHandle);
        node->RenderPass->BeginFrame();
    }
    m_Transfroms.clear();
}

void WorldRenderer::Render(EntityRegistry& registry)
{
    LNE_PROFILE_FUNCTION()
    auto& renderer = ApplicationBase::GetRenderer();
    vk::CommandBuffer cmdBuffer = renderer.GetGfxContext()->GetPrimaryCommandBuffer();

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

        for (auto& index : staticMeshView)
        {
            auto [transform, staticMesh] = staticMeshView.Get(index);

            // TODO: will probably insert frustum culling here
            auto& subMeshes = staticMesh.Mesh->GetSubMeshes();
            for (uint32_t i = 0; i < subMeshes.size(); ++i)
            {
                const SubMesh& subMesh = subMeshes[i];

                glm::mat4 model = transform.GetModelMatrix() * subMesh.WorldTransform;
                StaticMeshHash hash{ (uint64_t)staticMesh.Mesh.GetPtr(), i };
                m_Transfroms[hash].Transforms.emplace_back(model);

                for (auto& drawStaticMeshesAdder : drawStaticMeshesAdders)
                    drawStaticMeshesAdder->AddStaticMeshDrawCommand(hash, staticMesh.Mesh, i);
            }
        }

        uint32_t offset = 0;
        for (auto& [hash, subMeshArray] : m_Transfroms)
        {
            uint32_t size = (uint32_t)subMeshArray.Transforms.size();
            if (size == 0)
                continue;
            subMeshArray.Offset = offset;
            // copy submesh transforms to the transform buffer
            void* dst = m_TransformBuffers[renderer.GetCurrentFrameIndex()].Data + offset;
            std::memcpy(dst, subMeshArray.Transforms.data(), size * sizeof(glm::mat4));
            offset += size;
        }
        uint32_t totalSizeBytes = offset * sizeof(glm::mat4);
        m_TransformBuffers[renderer.GetCurrentFrameIndex()].Buffer->CopyData(
            cmdBuffer,
            m_TransformBuffers[renderer.GetCurrentFrameIndex()].Data, totalSizeBytes, 0);
    }
    renderer.PushLabel(cmdBuffer, "Frame");

    m_FrameGraph->Execute(cmdBuffer, this);

    renderer.PopLabel(cmdBuffer);
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
