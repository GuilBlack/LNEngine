#pragma once

#if defined(LNE_PLATFORM_WINDOWS)
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

namespace lne
{
void* OSAllocVPages(size_t size)
{
#ifdef LNE_PLATFORM_WINDOWS
    return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    // hope this is the right thing to do in linux since I can't test...
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? nullptr : p;
#endif // LNE_PLATFORM_WINDOWS
}

void OSFreeVPages(void* ptr, size_t size)
{
#if defined(LNE_PLATFORM_WINDOWS)
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, bytes);
#endif // LNE_PLATFORM_WINDOWS
}

std::size_t VPageSize()
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (std::size_t)info.dwPageSize;
#else
    long ps = sysconf(_SC_PAGESIZE);
    return ps > 0 ? (std::size_t)ps : 4096;
#endif
}

}
