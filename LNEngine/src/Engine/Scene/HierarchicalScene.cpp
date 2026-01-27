#include "HierarchicalScene.h"
#include "Engine/Graphics/Resources/Material.h"
#include "Engine/Graphics/Resources/Mesh.h"
#include "Engine/Graphics/Resources/Pipeline.h"
#include "Engine/Graphics/Resources/Texture.h"

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
        EntityRegistry::RegisterComponentType<LightComponent>();
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
