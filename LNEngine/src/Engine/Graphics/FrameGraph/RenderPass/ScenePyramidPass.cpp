#include "ScenePyramidPass.h"

#include "Graphics/WorldRenderer.h"
#include "Graphics/Renderer.h"
#include "Graphics/CommandBuffer.h"

#include "Graphics/FrameGraph/FrameGraph.h"

#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/Texture.h"
#include "Graphics/Resources/Mesh.h"

namespace lne
{
ScenePyramidPass::ScenePyramidPass(std::string_view passName, std::string_view inputTextureName)
{
    m_Name = passName;
    m_InputTextureName = inputTextureName;
}

void ScenePyramidPass::Execute(CommandBuffer* cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node)
{
    SafePtr<Texture> sceneTexture;
    for (auto res : node->InputResources)
    {
        FrameGraphResource* resource = frameGraph->GetResource(res);
        if (resource->Name == m_InputTextureName)
        {
            if (resource->Type == FrameGraphResourceType::eTexture || resource->Type == FrameGraphResourceType::eAttachment)
                sceneTexture = resource->Resource.GetAs<Texture>();
            break;
        }
    }
    if (sceneTexture)
        cmdBuffer->GenerateMips(sceneTexture.GetPtr());
}
}

