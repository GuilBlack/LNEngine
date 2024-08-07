#pragma once

namespace lne
{
class RefCountBase
{
public:
    virtual ~RefCountBase() = default;
    void Capture() const
#ifdef LNE_DEBUG
;
#else
    {
        ++m_Count;
    }
#endif // LNE_DEBUG

    uint32_t Release() const
#ifdef LNE_DEBUG
;
#else
    {
        assert(m_Count > 0);
        return --m_Count;
    }
#endif // LNE_DEBUG
    uint32_t GetCount() const
    {
        return m_Count.load();
    }
private:
    mutable std::atomic<uint32_t> m_Count = 0;
};

template<typename RefCountType>
class SafePtr
{
public:
    SafePtr()
    {
        static_assert(std::is_base_of_v<RefCountBase, RefCountType>, "RefCountType must derive from RefCountBase");
    }
    SafePtr(std::nullptr_t) 
    {
        static_assert(std::is_base_of_v<RefCountBase, RefCountType>, "RefCountType must derive from RefCountBase");
    }
    SafePtr(RefCountType* ptr)
        : m_Ptr(ptr)
    {
        static_assert(std::is_base_of_v<RefCountBase, RefCountType>, "RefCountType must derive from RefCountBase");
        IncreaseCount();
    }
    SafePtr(const SafePtr<RefCountType>& other)
        : m_Ptr(other.m_Ptr)
    {
        IncreaseCount();
    }
    SafePtr(SafePtr<RefCountType>&& other) noexcept
        : m_Ptr(other.m_Ptr)
    {
        other.m_Ptr = nullptr;
    }

    SafePtr<RefCountType>& operator=(const SafePtr<RefCountType>& other)
    {
        if (this == &other)
           return *this;
        DecreaseCount();
        m_Ptr = other.m_Ptr;
        IncreaseCount();
        return *this;
    }
    SafePtr<RefCountType>& operator=(SafePtr<RefCountType>&& other) noexcept
    {
        if (this == &other)
            return *this;
        DecreaseCount();
        m_Ptr = other.m_Ptr;
        other.m_Ptr = nullptr;
        return *this;
    }


    ~SafePtr()
    {
        DecreaseCount();
    }

    void Reset()
    {
        DecreaseCount();
        m_Ptr = nullptr;
    }
    void Reset(std::nullptr_t)
    {
        DecreaseCount();
        m_Ptr = nullptr;
    }
    void Reset(RefCountType* ptr)
    {
        if (m_Ptr != ptr)
        {
            DecreaseCount();
            m_Ptr = ptr;
            IncreaseCount();
        }
    }

    RefCountType* GetPtr() { return m_Ptr; }

    RefCountType* operator->() const
    {
        return m_Ptr;
    }
    RefCountType& operator*() const
    {
        return (*m_Ptr);
    }
    operator bool() const
    {
        return m_Ptr != nullptr;
    }
    bool operator==(const SafePtr<RefCountType>& other) const
    {
        return m_Ptr == other.m_Ptr;
    }
    bool operator==(const RefCountType* other)
    {
        return m_Ptr == other;
    }

    void IncreaseCount()
    {
        if (m_Ptr)
            m_Ptr->Capture();
    }

    void DecreaseCount()
    {
        if (m_Ptr)
        {
            if (m_Ptr->Release() == 0)
            {
                delete m_Ptr;
                m_Ptr = nullptr;
            }
        }
    }

private:
    RefCountType* m_Ptr{};
};
}
