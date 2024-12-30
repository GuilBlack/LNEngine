#pragma once

#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
#define lnnew   new( _NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define lnnew   new
#endif

namespace lne
{
using byte = uint8_t;
using SizeT = std::size_t;
}
#define MOVABLE_ONLY(T) \
    T(const T&) = delete; \
    T& operator=(const T&) = delete;
