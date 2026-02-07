#pragma once

#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
#define lnnew   new( _NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define lnnew   new
#endif

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;
using SizeT = std::size_t;

#define MOVABLE_ONLY(T) \
    T(const T&) = delete; \
    T& operator=(const T&) = delete;
