#pragma once
// This piece of code is from my other GitHub project: https://github.com/GuilBlack/ECS

#include "Types.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

namespace lne
{
struct IComponentStorage
{
    virtual ~IComponentStorage() = default;
};

template<ComponentConstraint Comp>
struct ComponentStorage : public IComponentStorage
{
    std::vector<Comp> Components;
};

class Archetype
{
public:
    Archetype() = default;
    ~Archetype() = default;

    void AddEntity(EntityID entity)
    {
        m_Entities.push_back(entity);
        m_EntityIndexMap[entity] = (uint32_t)m_Entities.size() - 1;
    }

    // Remove an entity from this archetype
    void RemoveEntity(EntityID entity)
    {
        if (!m_EntityIndexMap.contains(entity))
            return;

        uint32_t index = m_EntityIndexMap[entity];
        uint32_t lastIndex = (uint32_t)m_Entities.size() - 1;

        // Swap and pop to maintain contiguous storage
        if (index != lastIndex)
        {
            std::swap(m_Entities[index], m_Entities[lastIndex]);
            m_EntityIndexMap[m_Entities[index]] = index;
        }

        m_Entities.pop_back();
        m_EntityIndexMap.erase(entity);
    }

    // more for testing than anything else
    [[nodiscard]] bool HasEntity(EntityID entity) const
    {
        return m_EntityIndexMap.contains(entity);
    }

    [[nodiscard]] uint32_t GetEntityIndex(EntityID entity) const
    {
        assert(m_EntityIndexMap.contains(entity) && "This archetype doesn't contain this entity");
        return m_EntityIndexMap.at(entity);
    }

    [[nodiscard]] ECS_FORCE_INLINE const std::vector<EntityID>& GetEntities() const
    {
        return m_Entities;
    }

    [[nodiscard]] ECS_FORCE_INLINE std::vector<EntityID>* GetEntitiesPtr()
    {
        return &m_Entities;
    }

    [[nodiscard]] uint32_t GetEntityCount() const
    {
        return (uint32_t)m_Entities.size();
    }

    template<ComponentConstraint Comp, typename... Args>
    Comp& EmplaceComponent(EntityID entity, Args&&... args)
    {
        assert(m_EntityIndexMap.contains(entity) && "This archetype doesn't contain this entity");

        auto& compStorage = GetComponentStorage<Comp>();
        compStorage.Components.emplace_back(std::forward<Args>(args)...);
        return compStorage.Components.back();
    }

    template<ComponentConstraint Comp>
    void AddComponent(EntityID entity, const Comp& comp)
    {
        assert(m_EntityIndexMap.contains(entity) && "This archetype doesn't contain this entity");

        auto& compStorage = GetComponentStorage<Comp>();
        compStorage.Components.emplace_back(comp);
    }

    template<ComponentConstraint ...Comps>
    void AddComponents(EntityID entity, Comps... comps)
    {
        (AddComponent<Comps>(entity, comps), ...);
    }

    template<ComponentConstraint Comp>
    bool RemoveComponent(EntityID entity)
    {
        auto type = GetComponentTypeIndex<Comp>();
        if (!m_EntityIndexMap.contains(entity))
            return false;
        if (!m_ComponentStorages.contains(type))
            return false;

        auto& compStorate = *static_cast<ComponentStorage<Comp>*>(m_ComponentStorages[type].get());
        auto index = m_EntityIndexMap[entity];
        auto lastIndex = m_Entities.size() - 1;

        if (index != lastIndex)
        {
            std::swap(compStorate.Components[index], compStorate.Components[lastIndex]);
        }
        compStorate.Components.pop_back();
        return true;
    }

    template<ComponentConstraint ...Comps>
    void RemoveComponents(EntityID entity)
    {
        (RemoveComponent<Comps>(entity), ...);
    }

    template<ComponentConstraint Comp>
    [[nodiscard]] Comp& GetComponent(EntityID entity)
    {
        auto type = GetComponentTypeIndex<Comp>();
        assert(m_ComponentStorages.contains(type) && "Component storage doesn't exist.");
        auto& compStorage = *static_cast<ComponentStorage<Comp>*>(m_ComponentStorages[type].get());
        auto index = m_EntityIndexMap[entity];
        return compStorage.Components[index];
    }

    template<ComponentConstraint... Comps>
    [[nodiscard]] __inline std::tuple<Comps&...> GetComponents(EntityID entity)
    {
        return { GetComponent<Comps>(entity)... };
    }

    template<ComponentConstraint Comp>
    [[nodiscard]] Comp& GetComponentByIndex(uint32_t index)
    {
        auto type = GetComponentTypeIndex<Comp>();
        assert(m_ComponentStorages.contains(type) && "Component storage doesn't exist.");
        auto& compStorage = *static_cast<ComponentStorage<Comp>*>(m_ComponentStorages[type].get());
        return compStorage.Components[index];
    }

    template<ComponentConstraint Comp>
    [[nodiscard]] ComponentStorage<Comp>& GetComponentStorage()
    {
        auto type = GetComponentTypeIndex<Comp>();
        assert(m_ComponentStorages.contains(type) && "Component storage doesn't exist.");
        return *static_cast<ComponentStorage<Comp>*>(m_ComponentStorages[type].get());
    }

    template<ComponentConstraint... Comps>
    [[nodiscard]] std::tuple<ComponentStorage<Comps>&...> GetComponentStorages()
    {
        return { GetComponentStorage<Comps>()... };
    }

    template<ComponentConstraint Comp>
    void CreateComponentStorage()
    {
        auto type = GetComponentTypeIndex<Comp>();
        if (m_ComponentStorages.contains(type))
            return;
        m_ComponentStorages[type] = std::make_unique<ComponentStorage<Comp>>();
    }

    template<ComponentConstraint ...Comps>
    void CreateComponentStorages()
    {
        (CreateComponentStorage<Comps>(), ...);
    }

private:
    std::vector<EntityID>                                               m_Entities;
    FlatHashMap<EntityID, uint32_t>                                     m_EntityIndexMap;
    FlatHashMap<ComponentTypeIndex, std::unique_ptr<IComponentStorage>> m_ComponentStorages;

    friend class EntityRegistry;
};

template<ComponentConstraint... Comps>
class ComponentView
{
public:
    struct Index
    {
        EntityID Entity;
        uint32_t ArchetypeIndex;
        uint32_t ComponentIndex;
        uint32_t ComposedIndex; // Global index in the view

        Index(EntityID entity, uint32_t archetypeIndex, uint32_t componentIndex)
            : Entity(entity)
            , ArchetypeIndex(archetypeIndex)
            , ComponentIndex(componentIndex)
            , ComposedIndex(0) // Will be updated in the iterator
        {}

        bool operator==(const Index& other) const
        {
            return Entity == other.Entity
                && ArchetypeIndex == other.ArchetypeIndex
                && ComponentIndex == other.ComponentIndex;
            // Note: ComposedIndex is derived from the other three.
        }
    };

    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Index;
        using pointer = Index*;
        using reference = Index&;

        Iterator(Index index,
            uint32_t totalSize,
            const std::vector<uint32_t>& archetypeSizes,
            const std::vector<std::vector<EntityID>*>& entityData)
            : m_Index(index)
            , m_TotalSize(totalSize)
            , m_ArchetypeSizes(archetypeSizes)
            , m_EntityData(entityData)
        {
            UpdateComposedIndex();
        }

        reference operator*() { return m_Index; }
        pointer operator->() { return &m_Index; }

        Iterator& operator++()
        {
            if (++m_Index.ComponentIndex >= m_ArchetypeSizes[m_Index.ArchetypeIndex])
            {
                m_Index.ComponentIndex = 0;
                ++m_Index.ArchetypeIndex;
                if (m_Index.ArchetypeIndex >= m_ArchetypeSizes.size())
                {
                    m_Index.Entity = m_EntityData.back()->back();
                    m_Index.ComposedIndex = m_TotalSize;
                    return *this;
                }
            }
            m_Index.Entity = (*m_EntityData[m_Index.ArchetypeIndex])[m_Index.ComponentIndex];
            UpdateComposedIndex();
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator& operator+=(uint32_t n)
        {
            uint32_t numComponentsLeft = m_ArchetypeSizes[m_Index.ArchetypeIndex] - m_Index.ComponentIndex;
            if (n < numComponentsLeft)
            {
                m_Index.ComponentIndex += n;
                m_Index.Entity = (*m_EntityData[m_Index.ArchetypeIndex])[m_Index.ComponentIndex];
                UpdateComposedIndex();
                return *this;
            }
            n -= numComponentsLeft;
            for (uint32_t i = m_Index.ArchetypeIndex + 1; i < m_ArchetypeSizes.size(); ++i)
            {
                if (n < m_ArchetypeSizes[i])
                {
                    m_Index.ArchetypeIndex = i;
                    m_Index.ComponentIndex = n;
                    m_Index.Entity = (*m_EntityData[m_Index.ArchetypeIndex])[m_Index.ComponentIndex];
                    UpdateComposedIndex();
                    return *this;
                }
                n -= m_ArchetypeSizes[i];
            }
            throw std::out_of_range("Iterator out of range");
        }

        Iterator operator+(uint32_t n) const
        {
            Iterator tmp = *this;
            return tmp += n;
        }

        friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_Index != b.m_Index; }

    private:
        void UpdateComposedIndex()
        {
            if (m_Index.ArchetypeIndex >= m_ArchetypeSizes.size())
            {
                m_Index.ComposedIndex = m_TotalSize;
            }
            else
            {
                uint32_t prefix = 0;
                for (uint32_t i = 0; i < m_Index.ArchetypeIndex; i++)
                {
                    prefix += m_ArchetypeSizes[i];
                }
                m_Index.ComposedIndex = prefix + m_Index.ComponentIndex;
            }
        }

        Index m_Index;
        uint32_t m_TotalSize;
        const std::vector<uint32_t>& m_ArchetypeSizes;
        const std::vector<std::vector<EntityID>*>& m_EntityData;
    };

public:
    ComponentView(std::span<Archetype*> archetypeView)
    {
        m_ArchetypeSizes.reserve(archetypeView.size());
        for (Archetype* archetype : archetypeView)
        {
            auto entityData = archetype->GetEntitiesPtr();
            if (entityData->empty())
                continue;
            m_EntityData.push_back(entityData);
            uint32_t count = archetype->GetEntityCount();
            m_ArchetypeSizes.push_back(count);
            m_TotalSize += count;
            m_ComponentData.push_back(archetype->GetComponentStorages<Comps...>());
        }
    }

    ~ComponentView() = default;

    // Retrieve components for the given index.
    ECS_FORCE_INLINE constexpr std::tuple<Comps&...> Get(const Index& index)
    {
        return { std::get<ComponentStorage<Comps>&>(m_ComponentData[index.ArchetypeIndex]).Components[index.ComponentIndex]... };
    }

    ECS_FORCE_INLINE constexpr uint32_t TotalSize() const { return m_TotalSize; }

    ECS_FORCE_INLINE Iterator begin() const
    {
        return Iterator{ Index{ m_EntityData.front()->front(), 0, 0 },
                         m_TotalSize,
                         m_ArchetypeSizes,
                         m_EntityData };
    }

    ECS_FORCE_INLINE Iterator end() const
    {
        return Iterator{ Index{ m_EntityData.back()->back(), static_cast<uint32_t>(m_ArchetypeSizes.size()), 0 },
                         m_TotalSize,
                         m_ArchetypeSizes,
                         m_EntityData };
    }

    ECS_FORCE_INLINE constexpr Index operator[](uint32_t composedIndex) const
    {
        if (composedIndex >= m_TotalSize)
            throw std::out_of_range("ComponentView index out of range");
        uint32_t runningSum = 0;
        for (uint32_t i = 0; i < m_ArchetypeSizes.size(); i++)
        {
            if (composedIndex < runningSum + m_ArchetypeSizes[i])
            {
                uint32_t compIndex = composedIndex - runningSum;
                return Index{ (*m_EntityData[i])[compIndex], i, compIndex };
            }
            runningSum += m_ArchetypeSizes[i];
        }
        throw std::out_of_range("ComponentView index out of range");
    }

private:
    std::vector<std::vector<EntityID>*> m_EntityData;
    std::vector<std::tuple<ComponentStorage<Comps>&...>> m_ComponentData;
    std::vector<uint32_t> m_ArchetypeSizes;
    uint32_t m_TotalSize{ 0 };

    friend class EntityRegistry;
};
}
