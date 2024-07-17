#pragma once

#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
#define lnnew   new( _NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define lnnew   new
#endif

template<typename T>
using OwnedPtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

using byte = uint8_t;
