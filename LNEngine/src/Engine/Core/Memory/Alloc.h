#pragma once
#include "Engine/GlobalUtils.h"

#define                     GetKiloByte(numKB) numKB * 1024
#define                     GetMegaByte(numKB) numKB * 1024 * 1024
#define                     GetGigaByte(numKB) numKB * 1024 * 1024 * 1024

namespace lne
{
void*                       OSAllocVPages(size_t size, const char* file, int line);
void                        OSFreeVPages(void* ptr, size_t size);

void                        CheckForVLeaks();

std::size_t                 VPageSize();

inline constexpr std::size_t RoundUpToVPages(std::size_t bytes)
{
    return GlobalUtils::AlignmentRoundUp(bytes, VPageSize());
}
}

#define LneVirtualAlloc(size)       lne::OSAllocVPages(size, __FILE__, __LINE__)
#define LneVirtualFree(ptr, size)   lne::OSFreeVPages(ptr, size)
