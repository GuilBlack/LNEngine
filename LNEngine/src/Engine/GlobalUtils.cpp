#include "GlobalUtils.h"

namespace lne 
{
void GlobalUtils::PrintLine(const std::string& msg) 
{
    std::cout << msg << '\n';
}

std::size_t GlobalUtils::hash_u64(uint64_t value) noexcept
{
#if SIZE_MAX == UINT64_MAX
    // SplitMix64
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return static_cast<std::size_t>(value);
#else
    // 32-bit fallback Murmur3 fmix32-style
    uint32_t y = static_cast<uint32_t>(value) ^ static_cast<uint32_t>(value >> 32);
    y ^= y >> 16;
    y *= 0x85ebca6bU;
    y ^= y >> 13;
    y *= 0xc2b2ae35U;
    y ^= y >> 16;
    return static_cast<std::size_t>(y);
#endif
}

}