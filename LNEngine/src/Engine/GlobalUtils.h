#pragma once
#include "Engine/Core/Utils/Defines.h"

namespace lne 
{
class GlobalUtils
{
public:
    static std::size_t HashU64(u64 value) noexcept;

    static size_t NextPow2(size_t v)
    {
        if (v <= 1) return 1;
        --v;
        v |= v >> 1;  v |= v >> 2;  v |= v >> 4;  v |= v >> 8;  v |= v >> 16;
    #if SIZE_MAX > 0xFFFFFFFFu
        v |= v >> 32;
    #endif
        return v + 1;
    }

    template <class T>
    static void HashCombine(std::size_t& seed, T const& v) noexcept
    {
        std::hash<T> h;
        // same mixing as boost::hash_combine
        seed ^= h(v) + 0xe07e38bf4d17295ull + (seed << 6) + (seed >> 2);
    }

    template <class Enum>
    static std::size_t HashEnum(Enum e) noexcept
    {
        using U = std::underlying_type_t<Enum>;
        return std::hash<U>{}(static_cast<U>(e));
    }

    template <class T>
    static void HashPtr(std::size_t& seed, T const* p) noexcept
    {
        HashCombine(seed, reinterpret_cast<std::uintptr_t>(p));
    }

    static constexpr std::size_t AlignmentRoundUp(std::size_t size, std::size_t alignment)
    {
        return (size + alignment - 1) / alignment * alignment;
    }

    // faster than the normal but only works with alignments that are pow of 2 (so in theory, any alignment)
    static constexpr std::size_t PowOf2AlignmentRoundUp(std::size_t size, std::size_t alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};
}
