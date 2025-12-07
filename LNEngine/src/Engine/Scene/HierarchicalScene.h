#pragma once
#include "Components.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/ECS/EntityRegistry.h"
#include "Entity.h"

namespace lne
{

class HierarchicalScene : public RefCountBase
{
    using EntityIDMap = std::map<EntityID, Entity>;
public:
    HierarchicalScene();

    Entity CreateEntity(std::string name = "")
    {
        Entity entity{ m_EntityRegistry.CreateEntity(), &m_EntityRegistry };
        entity.EmplaceComponent<TransformComponent>();
        m_EntityIDMap[entity.GetID()] = entity;

        if (!name.empty())
            entity.EmplaceComponent<NameComponent>().Name = name;

        return entity;
    }

    void RemoveEntity(Entity entity)
    {
        m_EntityRegistry.DeleteEntity(entity.GetID());
        m_EntityIDMap.erase(entity.GetID());
    }

    void BeginScene();
    void EndScene();

public:
    operator EntityRegistry& () { return m_EntityRegistry; }
    operator const EntityRegistry& () const { return m_EntityRegistry; }

private:
    EntityRegistry m_EntityRegistry{ 8192 };
    EntityIDMap m_EntityIDMap;
    static bool s_Initialized;
};
}
