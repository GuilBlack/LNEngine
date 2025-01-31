#pragma once
#include "Engine/Core/Utils/Log.h"
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/ECS/Types.h"
#include "Engine/ECS/EntityRegistry.h"

namespace lne
{
class Entity
{
public:
    Entity() = default;
    Entity(EntityID handle, EntityRegistry* registry)
        : m_Handle(handle)
        , m_Registry(registry)
    {}

    template<ComponentConstraint Comp, typename... Args>
    Comp& EmplaceComponent(Args&&... args)
    {
        LNE_ASSERT(IsValid(), "Entity is not valid");
        return m_Registry->EmplaceComponent<Comp>(m_Handle, std::forward<Args>(args)...);
    }

    template<ComponentConstraint Comp>
    bool AddComponent(const Comp& comp)
    {
        LNE_ASSERT(IsValid(), "Entity is not valid");
        return m_Registry->TryAddComponent<Comp>(m_Handle, comp);
    }

    template<ComponentConstraint Comp>
    void RemoveComponent()
    {
        LNE_ASSERT(IsValid(), "Entity is not valid");
        m_Registry->DeleteComponent<Comp>(m_Handle);
    }

    template<ComponentConstraint Comp>
    Comp& GetComponent()
    {
        LNE_ASSERT(IsValid(), "Entity is not valid");
        return m_Registry->GetComponent<Comp>(m_Handle);
    }

    template<ComponentConstraint... Comps>
    auto GetComponents()
    {
        LNE_ASSERT(IsValid(), "Entity is not valid");
        return m_Registry->GetComponents<Comps...>(m_Handle);
    }

    template<ComponentConstraint... Comps>
    bool HasComponents()
    {
        LNE_ASSERT(IsValid(), "Entity is not valid");
        return m_Registry->HasComponents<Comps...>(m_Handle);
    }

    bool IsValid() const
    {
        return m_Handle != INVALID_ENTITY_ID && m_Registry != nullptr;
    }

    EntityID GetID() const { return m_Handle; }

private:
    EntityID m_Handle{ INVALID_ENTITY_ID };
    EntityRegistry* m_Registry{ nullptr };
    
    inline static std::string m_DefaultName{"Entity"};
};
}
