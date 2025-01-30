#pragma once
#include "Components.h"
#include "Engine/ECS/EntityRegistry.h"

namespace lne
{
class HierarchicalScene
{
public:
    HierarchicalScene();

private:
    EntityRegistry m_EntityRegistry{ 8192 };
};
}
