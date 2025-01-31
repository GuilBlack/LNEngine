#pragma once
#include "Components.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/ECS/EntityRegistry.h"
#include "Entity.h"

namespace lne
{

class HierarchicalScene : RefCountBase
{
    using EntityIDMap = std::map<EntityID, Entity>;
public:
    HierarchicalScene();

    Entity CreateEntity(std::string name = "Entity")
    {
        Entity entity{ m_EntityRegistry.CreateEntity(), &m_EntityRegistry };
        entity.EmplaceComponent<TransformComponent>();
        m_EntityIDMap[entity.GetID()] = entity;
        return entity;
    }

    void DestroyEntity(Entity entity)
    {
        m_EntityRegistry.DeleteEntity(entity.GetID());
        m_EntityIDMap.erase(entity.GetID());
    }

public:
    operator EntityRegistry& () { return m_EntityRegistry; }
    operator const EntityRegistry& () const { return m_EntityRegistry; }

private:
    EntityRegistry m_EntityRegistry{ 8192 };
    EntityIDMap m_EntityIDMap;
};
}
