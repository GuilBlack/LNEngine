#pragma once
#include "Alloc.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"
#include "Engine/Core/Utils/Log.h"

#if defined(LNE_PLATFORM_WINDOWS)
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/mman.h>
#endif


namespace lne
{
struct VirtualAllocInfo
{
    void*           Ptr;
    size_t          Size;
    const char*     FileName;
    int             Line;
};

static std::mutex g_VAMutex;
static FlatHashMap<void*, VirtualAllocInfo> g_VirtualAllocs;

void* OSAllocVPages(size_t size, const char* file, int line)
{
#ifdef LNE_PLATFORM_WINDOWS
    void* p = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#ifdef LNE_DEBUG
    if (p)
    {
        std::lock_guard<std::mutex> lock(g_VAMutex);
        g_VirtualAllocs[p] = { p, size, file, line };
    }
#endif
    return p;
#else
    // hope this is the right thing to do in linux since I can't test...
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#ifdef LNE_DEBUG
    if (p)
    {
        std::lock_guard<std::mutex> lock(g_VAMutex);
        g_VirtualAllocs[p] = { p, size };
    }
#endif
    return (p == MAP_FAILED) ? nullptr : p;
#endif // LNE_PLATFORM_WINDOWS
}

void OSFreeVPages(void* ptr, size_t size)
{
    if (ptr == nullptr)
        return;
#ifdef LNE_DEBUG
    {
        std::lock_guard<std::mutex> lock(g_VAMutex);
        auto it = g_VirtualAllocs.find(ptr);
        if (it == g_VirtualAllocs.end())
            __debugbreak();
        else
            g_VirtualAllocs.erase(it);
    }
#endif // LNE_DEBUG
#if defined(LNE_PLATFORM_WINDOWS)
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, bytes);
#endif // LNE_PLATFORM_WINDOWS
}

void CheckForVLeaks()
{
    for (auto& [ptr, info] : g_VirtualAllocs)
    {
        LNE_ERROR("Leaked VirtualAlloc: %p (%zu bytes) %s:%d\n",
                  ptr, info.Size, info.FileName, info.Line);
    }
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
