#pragma once

#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
#define lnnew   new( _NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define lnnew   new
#endif

using byte = uint8_t;
