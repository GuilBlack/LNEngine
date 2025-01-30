#pragma once
// This piece of code is from my other GitHub project: https://github.com/GuilBlack/ECS

namespace lne
{
class CircularBufferException : public std::exception
{
public:
    explicit CircularBufferException(const char* message) noexcept
        : exception(message)
    {}
};

template<typename T>
class CircularBuffer
{
public:
    /// <summary>
    /// Constructor for the CircularBuffer class.
    /// </summary>
    /// <param name="capacity"> is the size reserved for the buffer containing all the data. Default = 16</param>
    explicit CircularBuffer(uint32_t capacity = 16) noexcept
        : m_Head(0)
        , m_Tail(0)
        , m_Count(0)
        , m_Capacity(capacity)
    {
        m_Buffer = new T[capacity];
    }

    /// <summary>
    /// Constructor for the CircularBuffer class.
    /// </summary>
    /// <param name="other">: rvalue CircularBuffer that will be empty after the operation.</param>
    explicit CircularBuffer(CircularBuffer&& other) noexcept
        : m_Buffer(other.m_Buffer)
        , m_Head(other.m_Head)
        , m_Tail(other.m_Tail)
        , m_Count(other.m_Count)
        , m_Capacity(other.m_Capacity)
    {
        other.m_Buffer = nullptr;
        other.m_Head = 0;
        other.m_Tail = 0;
        other.m_Count = 0;
        other.m_Capacity = 0;
    }

    /// <summary>
    /// Constructor for the CircularBuffer class.
    /// </summary>
    /// <param name="other">: CircularBuffer to be copied.</param>
    CircularBuffer(const CircularBuffer& other) noexcept
        : m_Buffer(new T[other.m_Capacity])
        , m_Head(0)
        , m_Tail(other.m_Count % other.m_Capacity)
        , m_Count(other.m_Count)
        , m_Capacity(other.m_Capacity)
    {
        static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible.");
        if (other.m_Head < other.m_Tail)
        {
            std::copy(other.m_Buffer + other.m_Head, other.m_Buffer + other.m_Tail, m_Buffer);
        }
        else
        {
            auto newBufferMid = std::copy(other.m_Buffer + other.m_Head, other.m_Buffer + other.m_Capacity, m_Buffer);
            std::copy(other.m_Buffer, other.m_Buffer + (other.m_Count - (other.m_Capacity - other.m_Head)), newBufferMid);
        }
    }

    /// <summary>
    /// Destructor for the CircularBuffer class.
    /// </summary>
    ~CircularBuffer()
    {
        delete[] m_Buffer;
    }

    /// <summary>
    /// Add an item to the end of the buffer. 
    /// It will also automatically resize the buffer if it is full.
    /// It will throw an exception if the capacity is equal to the max of the uint32 type.
    /// </summary>
    void PushBack(const T& item)
    {
        CheckForResize();

        m_Buffer[m_Tail] = item;
        m_Tail = (m_Tail + 1) % m_Capacity;
        m_Count++;
    }

    /// <summary>
    /// Construct an item to the end of the buffer. 
    /// It will also automatically resize the buffer if it is full.
    /// It will throw an exception if the capacity is equal to the max of the uint32 type.
    /// </summary>
    template<typename... Args>
        requires IsConstructibleConstraint<T, Args...>
    void PushBack(Args&&... args)
    {
        CheckForResize();

        m_Buffer[m_Tail] = T(std::forward<Args>(args)...);
        m_Tail = (m_Tail + 1) % m_Capacity;
        m_Count++;
    }

    /// <summary>
    /// Add an item to the front of the buffer. 
    /// It will also automatically resize the buffer if it is full.
    /// It will throw an exception if the capacity is equal to the max of the uint32 type.
    /// </summary>
    void PushFront(const T& item)
    {
        CheckForResize();

        m_Head = (m_Head - 1 + m_Capacity) % m_Capacity;
        m_Buffer[m_Head] = item;
        m_Count++;
    }

    /// <summary>
    /// Add an item to the front of the buffer. 
    /// It will also automatically resize the buffer if it is full.
    /// It will throw an exception if the capacity is equal to the max of the uint32 type.
    /// </summary>
    template<typename... Args>
        requires IsConstructibleConstraint<T, Args...>
    void PushFront(Args&&... args)
    {
        CheckForResize();

        m_Head = (m_Head - 1 + m_Capacity) % m_Capacity;
        m_Buffer[m_Head] = T(std::forward<Args>(args)...);
        m_Count++;
    }

    /// <summary>
    /// Remove and return the last element from the buffer
    /// </summary>
    T PopBack()
    {
        static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>, "T must be move or copy assignable.");
        if (m_Count == 0)
            throw CircularBufferException("CircularBuffer is empty");

        m_Tail = (m_Tail - 1 + m_Capacity) % m_Capacity;
        if constexpr (std::is_move_assignable_v<T>)
        {
            T item = std::move(m_Buffer[m_Tail]);
            m_Count--;
            return item;
        }
        else
        {
            T item = m_Buffer[m_Tail];
            m_Count--;
            return item;
        }
    }

    /// <summary>
    /// Remove and return the first element from the buffer
    /// </summary>
    T PopFront()
    {
        static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>, "T must be move or copy assignable.");
        if (m_Count == 0)
            throw CircularBufferException("CircularBuffer is empty");

        if constexpr (std::is_move_assignable_v<T>)
        {
            T item = std::move(m_Buffer[m_Head]);
            m_Head = (m_Head + 1) % m_Capacity;
            m_Count--;
            return item;
        }
        else
        {
            T item = m_Buffer[m_Head];
            m_Head = (m_Head + 1) % m_Capacity;
            m_Count--;
            return item;
        }
    }

    /// <summary>
    /// Return the first item in the buffer
    /// </summary>
    constexpr T& GetFront() { 
        if (m_Count == 0)
            throw CircularBufferException("CircularBuffer is empty");
        return m_Buffer[m_Head]; 
    }

    /// <summary>
    /// Return the last item in the buffer
    /// </summary>
    constexpr T& GetBack()
    {
        if (m_Count == 0)
            throw CircularBufferException("CircularBuffer is empty");
        return m_Buffer[(m_Tail - 1 + m_Capacity) % m_Capacity];
    }

    /// <summary>
    /// Return the first item in the buffer
    /// </summary>
    constexpr const T& GetFront() const
    {
        if (m_Count == 0)
            throw CircularBufferException("CircularBuffer is empty");
        return m_Buffer[m_Head];
    }

    /// <summary>
    /// Return the last item in the buffer
    /// </summary>
    constexpr const T& GetBack() const
    {
        if (m_Count == 0)
            throw CircularBufferException("CircularBuffer is empty");
        return m_Buffer[(m_Tail - 1 + m_Capacity) % m_Capacity];
    }

    /// <summary>
    /// Change the capacity of the buffer, reallocating if necessary.
    /// If the new capacity is less than or equal to the current count, 
    /// this function does nothing to the buffer.
    /// If the new capacity is more than the current count but less than
    /// the current capacity, the buffer will be truncated.
    /// </summary>
    /// <param name="newCapacity"> the new capacity of the buffer</param>
    void Resize(uint32_t newCapacity)
    {
        if (newCapacity < m_Count)
            return;

        T* newBuffer = new T[newCapacity];
        if constexpr (std::is_move_constructible<T>::value && noexcept(T(std::declval<T>())))
        {
            if (m_Head < m_Tail)
            {
                std::move(m_Buffer + m_Head, m_Buffer + m_Tail, newBuffer);
            }
            else if (m_Head > m_Tail)
            {
                auto newBufferMid = std::move(m_Buffer + m_Head, m_Buffer + m_Capacity, newBuffer);
                std::move(m_Buffer, m_Buffer + (m_Count - (m_Capacity - m_Head)), newBufferMid);
            }
            else if (m_Count == m_Capacity)
            {
                auto newBufferMid = std::move(m_Buffer + m_Head, m_Buffer + m_Capacity, newBuffer);
                std::move(m_Buffer, m_Buffer + (m_Count - (m_Capacity - m_Head)), newBufferMid);
            }
        }
        else
        {
            if (m_Head < m_Tail)
            {
                std::copy(m_Buffer + m_Head, m_Buffer + m_Tail, newBuffer);
            }
            else if (m_Head > m_Tail)
            {
                auto newBufferMid = std::copy(m_Buffer + m_Head, m_Buffer + m_Capacity, newBuffer);
                std::copy(m_Buffer, m_Buffer + (m_Count - (m_Capacity - m_Head)), newBufferMid);
            }
            else if (m_Count == m_Capacity)
            {
                auto newBufferMid = std::copy(m_Buffer + m_Head, m_Buffer + m_Capacity, newBuffer);
                std::copy(m_Buffer, m_Buffer + (m_Count - (m_Capacity - m_Head)), newBufferMid);
            }
        }

        m_Capacity = newCapacity; 
        T* oldBuffer = m_Buffer;
        m_Buffer = newBuffer;
        delete[] oldBuffer;

        m_Head = 0;
        m_Tail = m_Count % m_Capacity;
    }

    /// <summary>
    /// Check if buffer is empty
    /// </summary>
    [[nodiscard]] inline constexpr bool IsEmpty() const noexcept { return m_Count == 0; }
    /// <summary>
    /// Check if the buffer is full
    /// </summary>
    [[nodiscard]] inline constexpr bool IsFull() const noexcept { return m_Count == m_Capacity; }

    /// <summary>
    /// Return the size of the buffer
    /// </summary>
    [[nodiscard]] inline constexpr uint32_t GetSize() const noexcept { return m_Count; }
    /// <summary>
    /// Return the capacity of the buffer
    /// </summary>
    [[nodiscard]] inline constexpr uint32_t GetCapacity() const noexcept { return m_Capacity; }

    void Clear() noexcept
    {
        if constexpr (std::is_destructible_v<T>)
        {
            for (uint32_t i = 0; i < m_Count; i++)
            {
                m_Buffer[(m_Head + i) % m_Capacity].~T();
            }
        }

        m_Head = 0;
        m_Tail = 0;
        m_Count = 0;
    }

#pragma region IteratorLogic
    class Iterator
    {
    public:
        Iterator(T* buffer, uint32_t index, uint32_t head, uint32_t capacity) noexcept
            : m_Buffer(buffer)
            , m_Index(index)
            , m_Head(head)
            , m_Capacity(capacity)
        {}

        T& operator*() { return m_Buffer[(m_Head + m_Index) % m_Capacity]; }

        const Iterator& operator++() noexcept
        {
            ++m_Index;
            return *this;
        }

        bool operator!=(const Iterator& other) const noexcept { return m_Index != other.m_Index; }

    private:
        T* m_Buffer;
        uint32_t m_Index;
        uint32_t m_Head;
        uint32_t m_Capacity;
    };

    Iterator begin() noexcept { return Iterator(m_Buffer, 0, m_Head, m_Capacity); }
    Iterator end() noexcept { return Iterator(m_Buffer, m_Count, m_Head, m_Capacity); }

    class ConstIterator
    {
    public:
        ConstIterator(const T* buffer, uint32_t index, uint32_t head, uint32_t capacity) noexcept
            : m_Buffer(buffer)
            , m_Index(index)
            , m_Head(head)
            , m_Capacity(capacity)
        {}

        const T& operator*() const { return m_Buffer[(m_Head + m_Index) % m_Capacity]; }

        const ConstIterator& operator++() noexcept
        {
            ++m_Index;
            return *this;
        }

        bool operator!=(const ConstIterator& other) const noexcept { return m_Index != other.m_Index; }

    private:
        const T* m_Buffer;
        uint32_t m_Index;
        uint32_t m_Head;
        uint32_t m_Capacity;
    };

    ConstIterator begin() const noexcept { return ConstIterator(m_Buffer, 0, m_Head, m_Capacity); }
    ConstIterator end() const noexcept { return ConstIterator(m_Buffer, m_Count, m_Head, m_Capacity); }
#pragma endregion

#pragma region OperatorLogic
    [[nodiscard]] constexpr T& operator[](uint32_t index) {
        if (index >= m_Count)
            throw CircularBufferException("Index out of range");
        return m_Buffer[(m_Head + index) % m_Capacity];
    }
    [[nodiscard]] constexpr const T& operator[](uint32_t index) const {
        if (index >= m_Count)
            throw CircularBufferException("Index out of range");
        return m_Buffer[(m_Head + index) % m_Capacity];
    }

    CircularBuffer& operator=(const CircularBuffer& other) noexcept
    {
        static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
        if (this != other)
        {
            delete[] m_Buffer;

            m_Capacity = other.m_Capacity;
            m_Count = other.m_Count;
            m_Head = 0;
            m_Tail = other.m_Tail % other.m_Capacity;

            m_Buffer = new T[m_Capacity];
            if (other.m_Head < other.m_Tail)
            {
                std::copy(other.m_Buffer + other.m_Head, other.m_Buffer + other.m_Tail, m_Buffer);
            }
            else
            {
                auto newBufferMid = std::copy(other.m_Buffer + other.m_Head, other.m_Buffer + other.m_Capacity, m_Buffer);
                std::copy(other.m_Buffer, other.m_Buffer + (other.m_Count - (other.m_Capacity - other.m_Head)), newBufferMid);
            }
        }
        return *this;
    }

    CircularBuffer& operator=(CircularBuffer&& other) noexcept
    {
        if (this != &other)
        {
            delete[] m_Buffer;

            m_Buffer = other.m_Buffer;
            m_Head = other.m_Head;
            m_Tail = other.m_Tail;
            m_Count = other.m_Count;
            m_Capacity = other.m_Capacity;

            other.m_Buffer = nullptr;
            other.m_Head = 0;
            other.m_Tail = 0;
            other.m_Count = 0;
            other.m_Capacity = 0;
        }

        return *this;
    }
#pragma endregion

private:
    T* m_Buffer;
    uint32_t m_Head;
    uint32_t m_Tail;
    uint32_t m_Count;
    uint32_t m_Capacity;

private:
    
    /// <summary>
    /// Resize helper method that checks if the buffer is full and needs to be resized
    /// </summary>
    void CheckForResize()
    {
        if (m_Count < m_Capacity)
            return;

        uint32_t newCapacity;
        if (m_Capacity == std::numeric_limits<uint32_t>::max())
            throw CircularBufferException("CircularBuffer is full and its capacity is at the limit");

        if (m_Capacity > std::numeric_limits<uint32_t>::max() / 2)
        {
            newCapacity = std::numeric_limits<uint32_t>::max();
        }
        else
        {
            newCapacity = uint32_t(m_Capacity * 2);
        }
        Resize(newCapacity);
    }
};
}
