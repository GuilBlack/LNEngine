#include "HierarchicalScene.h"
#include "../Graphics/Material.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/Texture.h"

namespace lne
{
HierarchicalScene::HierarchicalScene()
{
    EntityRegistry::RegisterComponentTypes<TransformComponent, CameraComponent, StaticMeshComponent>();
}
}
