#ifdef LNE_PLATFORM_WINDOWS
#define LNE_DEBUG_BREAK __debugbreak()
#elif defined(LNE_PLATFORM_LINUX)
#define LNE_DEBUG_BREAK __builtin_debugtrap()
#else
#define LNE_DEBUG_BREAK
#endif

#include "Defines.h"

#if defined(LNE_DEBUG)
#define LNE_ASSERT(x, ...) { if(!(x)) { LNE_ERROR("Assertion Failed: {}", __VA_ARGS__); __debugbreak(); } }
#else
#define LNE_ASSERT(x, ...) x
#endif
