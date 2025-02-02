#include "lnepch.h"
#include "BasicForwardPass.h"

#include <Core/ApplicationBase.h>
#include <Graphics/Renderer.h>
#include <Graphics/WorldRenderer.h>

namespace lne
{
void BasicForwardPass::BeginFrame()
{
    ClearDrawCommands();
}

void BasicForwardPass::Execute(vk::CommandBuffer commandBuffer, WorldRenderer* worldRenderer, FrameGraph* frameGraph,
    FrameGraphNode* node)
{
    auto& renderer = ApplicationBase::GetRenderer();
    for (auto& [hash, drawCommand] : m_DrawCommands)
    {
        SafePtr<StaticMesh> mesh = drawCommand.Mesh;
        SubMeshTransformArray& transforms = worldRenderer->GetTransforms(hash);
        TransformBuffer& transformBuffer = worldRenderer->GetTransformBuffer(renderer.GetCurrentFrameIndex());
        renderer.Draw(commandBuffer, drawCommand.Mesh, transformBuffer.Buffer, transforms.Offset, drawCommand.SubMeshIndex, drawCommand.InstanceCount);
    }
}
}
