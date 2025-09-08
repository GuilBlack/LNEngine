#pragma once
// This piece of code is from my other GitHub project: https://github.com/GuilBlack/ECS

#include "Archetype.h"
#include "Exceptions.h"
#include "Types.h"
#include "Engine/Core/DataStructures/CircularBuffer.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

namespace lne
{
// could have just used virtual functions in the IComponentStorage class but I wanted to try this approach for fun
using CreateStorageFunc = void(*)(Archetype*);
using RemoveComponentFunc = bool(*)(Archetype*, EntityID);
using MoveComponentFunc = void(*)(Archetype*, Archetype*, EntityID, EntityID);

class EntityRegistry
{
public:
    EntityRegistry()
        : m_AvailableEntities(m_MaxEntityCount)
        , m_EntitySignatures(m_MaxEntityCount)
    {
        Init();
    }

    uint32_t GetEntityCount() const { return m_EntityCount; }
    uint32_t GetMaxEntityCount() const { return m_MaxEntityCount; }

    template<ComponentConstraint... Comps>
    static void RegisterComponentTypes()
    {
        (RegisterComponentType<Comps>(), ...);
    }

    template<ComponentConstraint Comp>
    static void RegisterComponentType()
    {
        assert(GetComponentTypeIndex<Comp>() < 64 && "Too many components registered!");

        s_CreateStorageFuncs[GetComponentTypeIndex<Comp>()] = &CreateStorage<Comp>;
        s_RemoveComponentFuncs[GetComponentTypeIndex<Comp>()] = &RemoveComponent<Comp>;
        s_MoveComponentFuncs[GetComponentTypeIndex<Comp>()] = &MoveComponent<Comp>;
    }

    EntityRegistry(uint32_t MaxEntityCount)
        : m_MaxEntityCount(MaxEntityCount)
    {
        Init();
    }

    /// <summary>
    /// Creates an entity with no components attached to it.
    /// </summary>
    /// <returns>the ID of an entity</returns>
    const EntityID CreateEntity()
    {
        if (m_AvailableEntities.GetSize() == 0)
            Resize();
        const EntityID entity = m_AvailableEntities.PopFront();
        m_EntitySignatures[entity] = EntityMetadata{ EntitySignature(), GetArchetype(EntitySignature()) };
        ++m_EntityCount;
        return entity;
    }

    /// <summary>
    /// Deletes an entity with its components.
    /// </summary>
    /// <param name="entity">: ID of the entity to be deleted</param>
    ECS_FORCE_INLINE void DeleteEntity(EntityID entity)
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            return;
        m_DeletedEntities.PushBack(entity);
    }

    ECS_FORCE_INLINE bool IsEntityValid(EntityID entity)
    {
        return entity < m_MaxEntityCount && m_EntitySignatures[entity].Archetype != nullptr;
    }

    ///////////////////////////////////////////////////////////////////
    //// Component operations /////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////

    /// <summary>
    /// Deletes a component of the specified component type that has been added to the specified entity.
    /// </summary>
    /// <typeparam name="Comp">: Type of the component to be deleted.</typeparam>
    /// <param name="entity">: ID of the targeted entity</param>
    template<ComponentConstraint Comp>
    ECS_FORCE_INLINE void DeleteComponent(EntityID entity)
    {
        m_DeletedComponents.PushBack({ entity, GetComponentTypeIndex<Comp>() });
    }

    /// <summary>
    /// Adds a component that has already been created outside by you. Otherwise, use Emplace to create a new component that will be directly attached to the entity.
    /// </summary>
    /// <param name="entity">: ID of the entity that we want to modify</param>
    /// <param name="component">: Component to be added to the list</param>
    template<ComponentConstraint Comp>
    bool TryAddComponent(EntityID entity, const Comp& component) noexcept
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            return false;
        ComponentTypeID compType = ComponentType<Comp>();
        if ((m_EntitySignatures[entity].Signature & compType).any())
            return false;

        EntitySignature newSig = m_EntitySignatures[entity].Signature | compType;
        m_EntitySignatures[entity].Signature = newSig;

        Archetype* newArchetype = GetOrCreateArchetype(newSig);
        MigrateEntity(entity, m_EntitySignatures[entity].Archetype, newArchetype);
        newArchetype->AddComponent<Comp>(entity, component);
        return true;
    }

    /// <summary>
    /// Attaches a component of the specified type and constructed with the given arguments to an entity. If you have already constructed the component, use add component instead.
    /// </summary>
    /// <param name="entity">: ID of the entity that we want to modify</param>
    /// <param name="...args">arguments used to construct the component</param>
    /// <returns>the newly created component</returns>
    template<ComponentConstraint Comp, IsConstructibleConstraint... Args>
    Comp& EmplaceComponent(EntityID entity, Args&&... args)
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            throw EntityIDOutOfRange();

        ComponentTypeID compType = ComponentType<Comp>();
        if ((m_EntitySignatures[entity].Signature & compType).any())
            throw ComponentAlreadyExistsException();

        EntitySignature newSig = m_EntitySignatures[entity].Signature | compType;
        m_EntitySignatures[entity].Signature = newSig;

        Archetype* newArchetype = GetOrCreateArchetype(newSig);
        MigrateEntity(entity, m_EntitySignatures[entity].Archetype, newArchetype);
        return newArchetype->EmplaceComponent<Comp>(entity, std::forward<args>()...);
    }

    /// <summary>
    /// Replaces the component of the specified type of the given entity.
    /// </summary>
    /// <param name="entity">: ID of the entity that we want to modify</param>
    /// <param name="component">: New Component that will replace the current one</param>
    template<ComponentConstraint Comp>
    bool TryReplaceComponent(EntityID entity, const Comp& component)
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            return false;

        if (!m_EntitySignatures[entity].Signature.test(GetComponentTypeIndex<Comp>()))
            return false;

        Comp& comp = GetComponent<Comp>(entity);
        comp = component;
        return true;
    }

    /// <summary>
    /// Checks if the given entity has a component of the specified type.
    /// </summary>
    /// <typeparam name="Comp">: Type of the component that we want to check</typeparam>
    /// <param name="entity">: ID of the entity that we want to modify</param>
    /// <returns>a boolean that's true if the component has been found, false otherwise</returns>
    template<ComponentConstraint Comp>
    [[nodiscard]] ECS_FORCE_INLINE bool HasComponent(EntityID entity)
    {
        if (entity >= m_MaxEntityCount)
            throw EntityIDOutOfRange();

        return m_EntitySignatures[entity].Signature.test(GetComponentTypeIndex<Comp>());
    }

    /// <summary>
    /// Checks if the given entity has components of the specified types.
    /// </summary>
    /// <typeparam name="...Comps">: Types of the components that we want to check</typeparam>
    /// <param name="entity">: ID of the entity that we want to modify</param>
    /// <returns>a boolean that's true if the components have been found, false otherwise</returns>
    template<ComponentConstraint... Comps>
    [[nodiscard]] ECS_FORCE_INLINE bool HasComponents(EntityID entity)
    {
        return (HasComponent<Comps>(entity) && ...);
    }

    template<ComponentConstraint Comp>
    [[nodiscard]] Comp& TryGetComponent(EntityID entity, bool& isValid)
    {
        static Comp outComp;
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            throw EntityIDOutOfRange();

        if (m_EntitySignatures[entity].Signature.test(GetComponentTypeIndex<Comp>()))
        {
            outComp = Comp{};
            isValid = false;
        }

        isValid = true;
        return m_EntitySignatures[entity].Archetype->GetComponent<Comp>(entity);
    }

    template<ComponentConstraint Comp>
    [[nodiscard]] ECS_FORCE_INLINE Comp& GetComponent(EntityID entity)
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            throw EntityIDOutOfRange();
        if (!m_EntitySignatures[entity].Signature.test(GetComponentTypeIndex<Comp>()))
            throw NoComponentException();

        return m_EntitySignatures[entity].Archetype->GetComponent<Comp>(entity);
    }

    template<ComponentConstraint... Comps>
    [[nodiscard]] ECS_FORCE_INLINE std::tuple<Comps&...> GetComponents(EntityID entity)
    {
        return { GetComponent<Comps>(entity)... };
    }
    
    template<ComponentConstraint... Comps>
    [[nodiscard]] ComponentView<Comps...> GetView()
    {
        EntitySignature sig;
        sig |= (ComponentType<Comps>() | ...);
        if (!m_ArchetypeCache.contains(sig))
             CreateArchetypeCache(sig);
        auto it = m_Archetypes.find(sig);
        if (it == m_Archetypes.end())
            return ComponentView<Comps...>(std::span<Archetype*>());
        return ComponentView<Comps...>(m_ArchetypeCache[sig]);
    }

    void Flush()
    {
        for (int i = m_DeletedComponents.GetSize() - 1; i >= 0; --i)
        {
            auto entityComp = m_DeletedComponents.PopFront();
            DeleteComponent_Internal(entityComp.first, entityComp.second);
        }
        for (int i = m_DeletedEntities.GetSize() - 1; i >= 0; --i)
        {
            EntityID entity = m_DeletedEntities.PopFront();
            DeleteEntity_Internal(entity);
        }
    }

private:
    using ArchetypeMap = FlatHashMap<EntitySignature, std::unique_ptr<Archetype>,
        std::hash<EntitySignature>>;
    using ArchetypeCache = FlatHashMap<EntitySignature, std::vector<Archetype*>,
        std::hash<EntitySignature>>;

    uint32_t m_EntityCount = 0;
    uint32_t m_MaxEntityCount = 4096;

    CircularBuffer<EntityID>                                        m_AvailableEntities;
    struct EntityMetadata
    {
        EntitySignature Signature{};
        Archetype*      Archetype{};
    };
    std::vector<EntityMetadata>                                     m_EntitySignatures;
    ArchetypeMap                                                    m_Archetypes;
    ArchetypeCache                                                  m_ArchetypeCache; // list of archetypes that has AT LEAST these components for looping through entities faster

    CircularBuffer<EntityID> m_DeletedEntities;
    CircularBuffer<std::pair<EntityID, ComponentTypeIndex>>         m_DeletedComponents;

private:
    static inline std::array<CreateStorageFunc, MAX_COMPONENTS>     s_CreateStorageFuncs = {};
    static inline std::array<MoveComponentFunc, MAX_COMPONENTS>     s_MoveComponentFuncs = {};
    static inline std::array<RemoveComponentFunc, MAX_COMPONENTS>   s_RemoveComponentFuncs = {};

private:

    /// <summary>
    /// Initialization helper
    /// </summary>
    void Init()
    {
        m_EntitySignatures.resize(m_MaxEntityCount);
        for (EntityID i = 0; i < m_MaxEntityCount; ++i)
            m_AvailableEntities.PushBack(i);

        EntitySignature emptySig;
        m_Archetypes[emptySig].reset(new Archetype());
    }

    Archetype* GetArchetype(EntitySignature signature)
    {
        if (m_Archetypes.contains(signature))
            return m_Archetypes[signature].get();
        return nullptr;
    }

    Archetype* GetOrCreateArchetype(EntitySignature signature)
    {
        if (m_Archetypes.contains(signature))
            return m_Archetypes[signature].get();
        Archetype* archetype = new Archetype();

        for (uint32_t i = 0; i < MAX_COMPONENTS; ++i)
        {
            if (signature.test(i))
            {
                assert(s_CreateStorageFuncs[i] && "Component type not registered!");
                s_CreateStorageFuncs[i](archetype);
            }
        }
        m_Archetypes[signature].reset(archetype);

        for (auto& [sig, archetypes] : m_ArchetypeCache)
        {
            if ((signature & sig) == sig)
                archetypes.push_back(archetype);
        }

        return archetype;
    }

    void MigrateCommonComponents(
        Archetype* srcArchetype, Archetype* dstArchetype, 
        EntityID oldEntity, EntityID newEntity
    )
    {
        for (auto& [compType, _] : srcArchetype->m_ComponentStorages)
        {
            if (!dstArchetype->m_ComponentStorages.contains(compType))
                continue;
            assert(s_MoveComponentFuncs[compType] && "Component type not registered!");
            s_MoveComponentFuncs[compType](srcArchetype, dstArchetype, oldEntity, newEntity);
        }
    }

    void MigrateEntity(EntityID entity, Archetype* srcArchetype, Archetype* dstArchetype)
    {
        dstArchetype->AddEntity(entity);

        MigrateCommonComponents(srcArchetype, dstArchetype, entity, entity);

        srcArchetype->RemoveEntity(entity);
        m_EntitySignatures[entity].Archetype = dstArchetype;
    }

    void DeleteComponent_Internal(EntityID entity, ComponentTypeIndex compType)
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            return;
        assert(s_RemoveComponentFuncs[compType] && "Component type not registered!");
        if (!s_RemoveComponentFuncs[compType](m_EntitySignatures[entity].Archetype, entity))
            return;

        ComponentTypeID compId = ComponentTypeID((uint64_t)1 << (uint32_t)compType);

        EntitySignature newSig = m_EntitySignatures[entity].Signature & compId.flip();
        m_EntitySignatures[entity].Signature = newSig;

        Archetype* newArchetype = GetOrCreateArchetype(newSig);
        MigrateEntity(entity, m_EntitySignatures[entity].Archetype, newArchetype);
    }

    void DeleteEntity_Internal(EntityID entity)
    {
        if (entity >= m_MaxEntityCount || m_EntitySignatures[entity].Archetype == nullptr)
            return;

        Archetype* archetype = m_EntitySignatures[entity].Archetype;
        for (auto& [compType, _] : archetype->m_ComponentStorages)
        {
            assert(s_RemoveComponentFuncs[compType] && "Component type not registered!");
            s_RemoveComponentFuncs[compType](archetype, entity);
        }
        archetype->RemoveEntity(entity);
        m_EntitySignatures[entity].Archetype = nullptr;
        m_EntitySignatures[entity].Signature = EntitySignature();
        m_AvailableEntities.PushBack(entity);
        --m_EntityCount;
    }

    ECS_FORCE_INLINE void CreateArchetypeCache(EntitySignature cacheSig)
    {
        for (const auto&[sig, archetype] : m_Archetypes)
        {
            if ((cacheSig & sig) == cacheSig)
                m_ArchetypeCache[cacheSig].push_back(archetype.get());
        }
    }

private:
    template<ComponentConstraint Comp>
    static void CreateStorage(Archetype* archetype)
    {
        archetype->CreateComponentStorage<Comp>();
    }

    template<ComponentConstraint Comp>
    static void MoveComponent(Archetype* src, Archetype* dst, EntityID srcEntity, EntityID dstEntity)
    {
        dst->AddComponent<Comp>(dstEntity, src->GetComponent<Comp>(srcEntity));
        src->RemoveComponent<Comp>(srcEntity);
    }

    template<ComponentConstraint Comp>
    static bool RemoveComponent(Archetype* archetype, EntityID entity)
    {
        return archetype->RemoveComponent<Comp>(entity);
    }

    void Resize()
    {
        m_MaxEntityCount *= 2;
        m_AvailableEntities.Resize(m_MaxEntityCount);
        m_EntitySignatures.resize(m_MaxEntityCount);
        for (EntityID i = m_MaxEntityCount / 2; i < m_MaxEntityCount; ++i)
        {
            m_AvailableEntities.PushBack(i);
        }        
    }
};
}
