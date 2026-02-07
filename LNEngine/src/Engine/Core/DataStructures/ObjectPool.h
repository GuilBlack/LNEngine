#pragma once
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"
// !!!! THIS IS ONLY FOR DEFAULT CONSTRUCTIBLE TYPES !!!!

namespace lne
{
template <typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

using ObjectPoolHandle = u32;
static constexpr ObjectPoolHandle INVALID_OBJECT_POOL_HANDLE = (ObjectPoolHandle)-1;

template <DefaultConstructible ObjType>
class ObjectPool
{
public:

public:
    ObjectPool(SizeT capacity = 16)
        : m_Capacity{ capacity }, m_ObjectSize{ sizeof(ObjType) },
        m_PoolSize{ capacity * m_ObjectSize }
    {
        m_IsAllocated.resize(capacity, false);
        m_Pool = static_cast<u8*>(std::malloc(m_PoolSize));
    }

    ~ObjectPool()
    {
        std::sort(m_FreeIndices.begin(), m_FreeIndices.end());

        u32 holeIndex = 0;
        u32 holeCount = (u32)m_FreeIndices.size();
        // loop through all indices and deallocate them while skipping free indices
        for (u32 i = 0; i < m_NextIndex; ++i)
        {
            if (holeIndex < holeCount && m_FreeIndices[holeIndex] == i)
            {
                ++holeIndex;
                continue;
            }

            ObjType* ptr = GetPointerFromHandle(i);
            ptr->~ObjType();
        }

        std::free(m_Pool);
    }

    ObjectPoolHandle Allocate()
    {
        ObjectPoolHandle handle = INVALID_OBJECT_POOL_HANDLE;

        if (m_FreeIndices.empty())
        {
            if (m_NextIndex >= m_Capacity)
            {
                LNE_ERROR("Pool is full, cannot allocate!\n");
                return INVALID_OBJECT_POOL_HANDLE;
            }

            ObjType* ptr = GetPointerFromHandle(m_NextIndex);
            new (ptr) ObjType();

            handle = m_NextIndex++;
        }
        else
        {
            handle = m_FreeIndices.back();
            m_FreeIndices.pop_back();

            ObjType* ptr = GetPointerFromHandle(handle);
            new (ptr) ObjType();
        }

        m_IsAllocated[handle] = true;

        return handle;
    }

    void Deallocate(ObjectPoolHandle objHandle)
    {
        ObjType* ptr = Access(objHandle);
        if (!ptr)
            return;

        ptr->~ObjType();

        m_FreeIndices.push_back(objHandle);
        m_IsAllocated[objHandle] = false;
    }

    ObjType* Access(ObjectPoolHandle objHandle) const
    {
        if (objHandle == INVALID_OBJECT_POOL_HANDLE || objHandle >= m_Capacity)
        {
            LNE_WARN("Trying to get invalid handle!\n");
            return nullptr;
        }

        if (!m_IsAllocated[objHandle])
        {
            LNE_WARN("Trying to get handle that is not allocated!\n");
            return nullptr;
        }

        return GetPointerFromHandle(objHandle);
    }

private:
    SizeT m_Capacity;
    SizeT m_ObjectSize;

    SizeT m_PoolSize;
    u8* m_Pool{ nullptr };

    ObjectPoolHandle m_NextIndex{ 0 };
    std::vector<u32> m_FreeIndices;
    std::vector<bool> m_IsAllocated;

private:
    ObjType* GetPointerFromHandle(ObjectPoolHandle index) const
    {
        return reinterpret_cast<ObjType*>(m_Pool + index * m_ObjectSize);
    }
};
}

