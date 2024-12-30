#pragma once
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "ObjectPool.h"

namespace lne
{
template<typename KeyType, DefaultConstructible ValueType>
class ObjectCache
{
public:
    ObjectCache(SizeT capacity = 16)
        : m_Pool{ capacity }
    {}

    ObjectPool<ValueType>& GetPool() { return m_Pool; }
    const ObjectPool<ValueType>& GetPool() const { return m_Pool; }

    ObjectPoolHandle Insert(const KeyType& key)
    {
        auto it = m_Cache.find(key);
        if (it != m_Cache.end())
            return it->second;

        ObjectPoolHandle handle = m_Pool.Allocate();
        m_Cache[key] = handle;
        return handle;
    }

    void Remove(const KeyType& key)
    {
        auto it = m_Cache.find(key);
        if (it == m_Cache.end())
            return;

        m_Pool.Deallocate(it->second);
        m_Cache.erase(it);
    }

    ValueType* Access(const KeyType& key) const
    {
        auto it = m_Cache.find(key);
        if (it == m_Cache.end())
            return nullptr;

        return m_Pool.Get(it->second);
    }

    bool Contains(const KeyType& key) const
    {
        return (m_Cache.find(key) != m_Cache.end());
    }

private:
    ObjectPool<ValueType> m_Pool;
    std::unordered_map<KeyType, ObjectPoolHandle> m_Cache;
};
}

