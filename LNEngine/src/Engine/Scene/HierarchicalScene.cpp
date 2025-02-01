#include "HierarchicalScene.h"
#include "../Graphics/Material.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/Texture.h"

namespace lne
{
bool HierarchicalScene::s_Initialized = false;
HierarchicalScene::HierarchicalScene()
{
    if (!s_Initialized)
    {
        EntityRegistry::RegisterComponentType<TransformComponent>();
        EntityRegistry::RegisterComponentType<CameraComponent>();
        EntityRegistry::RegisterComponentType<StaticMeshComponent>();
        s_Initialized = true;
    }
}

void HierarchicalScene::BeginScene()
{}

void HierarchicalScene::EndScene()
{
    m_EntityRegistry.Flush();
}
}
