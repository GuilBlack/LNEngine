#pragma once
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "Engine/Core/Memory/SlabAllocator.h"
#include "Engine/GlobalUtils.h"
#include <new>
// !!!! THIS IS ONLY FOR DEFAULT CONSTRUCTIBLE TYPES !!!!

namespace lne
{
template <typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

using ObjectPoolHandle = void*;
static constexpr ObjectPoolHandle INVALID_OBJECT_POOL_HANDLE = nullptr;

/**
 * Object Pool used to store any type of object. It's your job to deallocate a used object
 * After you're done using it and before the pool is destroyed because it's prone to
 * UB behavior and leaks if not since the object is ONLY destroyed IF the Deallocate
 * method is called.
 */
template <typename ObjType>
class ObjectPool
{
public:
    // for bookkeeping unfortunately
    struct Node
    {
        Node*                       Next;
        Node*                       Prev;
        alignas(ObjType) u8         Allocation[sizeof(ObjType)];

        ObjType*                    GetObjectPtr()
        {
            // launder is used to make the compiler police happy about the object's lifetime and type
            return std::launder(reinterpret_cast<ObjType*>(Allocation));
        }

        const ObjType* GetObjectPtr() const
        {
            return std::launder(reinterpret_cast<const ObjType*>(Allocation));
        }
    };

public:
    /**
     * Constructor.
     * @param allocator OPTIONAL. If nullptr, it will create its own SlabAllocator. 
     * It's optional if we ever have the need for an external allocator.
     */
    explicit ObjectPool(SlabAllocator* allocator = nullptr)
    {
        constexpr std::size_t blockSize = GlobalUtils::AlignmentRoundUp(sizeof(Node), alignof(Node));
        constexpr std::size_t alignment = alignof(Node);

        if (allocator)
        {
            LNE_ASSERT(allocator->GetBlockSize() == blockSize &&
                       allocator->GetAlignment() == alignment,
                       std::format("SlabAllocator mismatch for ObjectPool."));
            m_Allocator = allocator;
            m_OwnsAllocator = false;
        }
        else
        {
            m_Allocator = lnnew SlabAllocator(blockSize, alignment);
            m_OwnsAllocator = true;
        }
    }

    ~ObjectPool()
    {
        if constexpr (std::is_trivial_v<ObjType> == false)
            Clear();

        if (m_OwnsAllocator)
            delete m_Allocator;
    }

    template <class... Args>
    ObjectPoolHandle Allocate(Args&&... args) requires std::constructible_from<ObjType, Args...>
    {
        Node* node = static_cast<Node*>(m_Allocator->Allocate());

        try
        {
            std::construct_at(node);
            std::construct_at(node->GetObjectPtr(), std::forward<Args>(args)...);
        }
        catch (...)
        {
            m_Allocator->Deallocate(node);
            throw;
        }

        LinkLive(node);
        return static_cast<ObjectPoolHandle>(node);
    }

    void Deallocate(ObjectPoolHandle objHandle)
    {
        if (objHandle == INVALID_OBJECT_POOL_HANDLE)
            return;

        Node* node = static_cast<Node*>(objHandle);

        UnlinkLive(node);

        if constexpr (!std::is_trivially_destructible_v<ObjType>)
            std::destroy_at(node->GetObjectPtr());

        std::destroy_at(node);
        m_Allocator->Deallocate(node);
    }

    ObjType*                    Access(ObjectPoolHandle objHandle) const
    {
        if (objHandle == INVALID_OBJECT_POOL_HANDLE)
            return nullptr;
        return static_cast<Node*>(objHandle)->GetObjectPtr();
    }

    void Clear()
    {
        Node* it = m_LiveHead;

        while (it)
        {
            Node* next = it->Next;

            if constexpr (!std::is_trivially_destructible_v<ObjType>)
                std::destroy_at(it->GetObjectPtr());

            std::destroy_at(it);
            m_Allocator->Deallocate(it);

            it = next;
        }

        m_LiveHead = nullptr;
        m_LiveTail = nullptr;
    }

    bool                        IsEmpty() const noexcept { return m_LiveHead == nullptr; }

    const SlabAllocator&        GetAllocator() const { return *m_Allocator; }
    bool                        OwnsAllocator() const { return m_OwnsAllocator; }

    // FOR DEBUGGING PURPOSES!!
    const Node* const           GetLiveHead() const { return m_LiveHead; }
    const Node* const           GetLiveTail() const { return m_LiveTail; }

private:
    SlabAllocator*          m_Allocator{};
    bool                    m_OwnsAllocator{};

    Node*                   m_LiveHead{};
    Node*                   m_LiveTail{};

private:
    void LinkLive(Node* node)
    {
        if (m_LiveHead == nullptr)
            m_LiveHead = node;
        node->Prev = m_LiveTail;
        node->Next = nullptr;
        if (m_LiveTail)
            m_LiveTail->Next = node;
        m_LiveTail = node;
    }

    void UnlinkLive(Node* node)
    {
        if (node->Prev)
            node->Prev->Next = node->Next;
        else
            m_LiveHead = node->Next;

        if (node->Next)
            node->Next->Prev = node->Prev;
        else
            m_LiveTail = node->Prev;
    }

private:
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
};
}

