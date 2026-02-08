#include "MemoryTest.h"
#include "Engine/Core/Memory/Alloc.h"
#include "Engine/Core/Memory/SlabAllocator.h"

namespace MemoryTest
{
static bool IsAlignedTo(std::uintptr_t value, std::size_t alignment)
{
    return alignment == 0 ? true : (value % alignment) == 0;
}

#pragma region VPageAllocTests

////////////////////////////////////////////////////////////////////////////////////////////////////////
//// VPageAllocTests ///////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(OSVPages, AllocReturnsNonNullAndIsWritable)
{
    const std::size_t ps = lne::VPageSize();
    const std::size_t bytes = lne::RoundUpToVPages(ps); // at least 1 page

    void* p = lne::OSAllocVPages(bytes);
    ASSERT_NE(p, nullptr);

    // Write/read pattern
    std::memset(p, 0xAB, bytes);
    unsigned char* b = static_cast<unsigned char*>(p);
    EXPECT_EQ(b[0], 0xAB);
    EXPECT_EQ(b[bytes - 1], 0xAB);

    lne::OSFreeVPages(p, bytes);
}

TEST(OSVPages, PointerIsPageAligned)
{
    using namespace lne;
    const std::size_t ps = VPageSize();
    const std::size_t bytes = RoundUpToVPages(ps);

    void* p = OSAllocVPages(bytes);
    ASSERT_NE(p, nullptr);

    EXPECT_TRUE(IsAlignedTo(reinterpret_cast<std::uintptr_t>(p), ps))
        << "Expected OSAllocVPages to return page-aligned address";

    OSFreeVPages(p, bytes);
}

TEST(OSVPages, TwoAllocationsDoNotOverlap)
{
    using namespace lne;
    const std::size_t ps = VPageSize();
    const std::size_t bytes = RoundUpToVPages(ps * 2);

    void* p1 = OSAllocVPages(bytes);
    void* p2 = OSAllocVPages(bytes);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);

    auto a1 = reinterpret_cast<std::uintptr_t>(p1);
    auto a2 = reinterpret_cast<std::uintptr_t>(p2);

    // Ensure [p1, p1+bytes) and [p2, p2+bytes) don't overlap.
    // Order-independent:
    std::uintptr_t loA = std::min(a1, a2);
    std::uintptr_t hiA = std::max(a1, a2);
    EXPECT_GE(hiA, loA + bytes);

    OSFreeVPages(p1, bytes);
    OSFreeVPages(p2, bytes);
}

TEST(OSVPages, ManyAllocationsAreWritableAndDistinct)
{
    using namespace lne;
    const std::size_t ps = VPageSize();
    const std::size_t bytes = RoundUpToVPages(ps);

    constexpr int kCount = 64;
    std::vector<void*> ptrs;
    ptrs.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        void* p = OSAllocVPages(bytes);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);

        // Write a unique byte so we touch the page.
        static_cast<unsigned char*>(p)[0] = static_cast<unsigned char>(i);
    }

    // Verify uniqueness (addresses distinct)
    std::vector<std::uintptr_t> addrs;
    addrs.reserve(ptrs.size());
    for (void* p : ptrs) addrs.push_back(reinterpret_cast<std::uintptr_t>(p));
    std::sort(addrs.begin(), addrs.end());
    auto it = std::adjacent_find(addrs.begin(), addrs.end());
    EXPECT_EQ(it, addrs.end());

    // Verify our writes
    for (int i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(static_cast<unsigned char*>(ptrs[i])[0], static_cast<unsigned char>(i));
    }

    for (void* p : ptrs)
        OSFreeVPages(p, bytes);
}

#pragma endregion

#pragma region SlabAllocatorTests

////////////////////////////////////////////////////////////////////////////////////////////////////////
//// SlabAllocatorTests ////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SlabAllocator_SingleThreaded, ReturnedPointersAreAligned)
{
    constexpr std::size_t kBlockSize = 64;
    constexpr std::size_t kAlignment = 32;
    constexpr std::size_t kSlabBytes = 64 * 1024;

    lne::SlabAllocator alloc(kBlockSize, kAlignment);

    for (int i = 0; i < 1000; ++i)
    {
        void* p = alloc.Allocate();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(IsAlignedTo(reinterpret_cast<std::uintptr_t>(p), kAlignment));
        alloc.Deallocate(p);
    }
}

TEST(SlabAllocator_SingleThreaded, AllocationsAreUniqueWhileLive)
{
    constexpr std::size_t kBlockSize = 64;
    constexpr std::size_t kAlignment = alignof(void*);
    constexpr std::size_t kSlabBytes = 64 * 1024;

    lne::SlabAllocator alloc(kBlockSize, kAlignment, kSlabBytes);

    std::vector<void*> ptrs;
    ptrs.reserve(2000);

    // Allocate a bunch without freeing; all should be unique.
    for (int i = 0; i < 2000; ++i)
    {
        void* p = alloc.Allocate();
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }

    std::vector<std::uintptr_t> addrs;
    addrs.reserve(ptrs.size());
    for (void* p : ptrs) addrs.push_back(reinterpret_cast<std::uintptr_t>(p));
    std::sort(addrs.begin(), addrs.end());

    auto dup = std::adjacent_find(addrs.begin(), addrs.end());
    EXPECT_EQ(dup, addrs.end()) << "Allocator returned the same block twice while live";

    for (void* p : ptrs) alloc.Deallocate(p);
}

TEST(SlabAllocator_SingleThreaded, ReusesFreedBlocksFirst)
{
    constexpr std::size_t kBlockSize = 64;
    constexpr std::size_t kAlignment = alignof(void*);
    constexpr std::size_t kSlabBytes = 64 * 1024;

    lne::SlabAllocator alloc(kBlockSize, kAlignment, kSlabBytes);

    void* a = alloc.Allocate();
    void* b = alloc.Allocate();
    void* c = alloc.Allocate();

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    // Free middle one, then allocate again; should return b (LIFO free list)
    alloc.Deallocate(b);

    void* d = alloc.Allocate();
    EXPECT_EQ(d, b) << "Expected allocator to reuse last-freed block first (LIFO)";

    // Cleanup
    alloc.Deallocate(a);
    alloc.Deallocate(c);
    alloc.Deallocate(d);
}

TEST(SlabAllocator_SingleThreaded, CanGrowBeyondOneSlab)
{
    constexpr std::size_t kBlockSize = 64;
    constexpr std::size_t kAlignment = alignof(void*);
    constexpr std::size_t kSlabBytes = 4096;

    lne::SlabAllocator alloc(kBlockSize, kAlignment, kSlabBytes);

    // Allocate enough blocks to exceed one slab capacity.
    // We don't know exact count because header + alignment reduce capacity.
    // So just allocate "a lot" and ensure we never get nullptr and uniqueness holds.
    constexpr int kCount = 2000;
    std::vector<void*> ptrs;
    ptrs.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        void* p = alloc.Allocate();
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }

    std::vector<std::uintptr_t> addrs;
    addrs.reserve(ptrs.size());
    for (void* p : ptrs) addrs.push_back(reinterpret_cast<std::uintptr_t>(p));
    std::sort(addrs.begin(), addrs.end());
    auto dup = std::adjacent_find(addrs.begin(), addrs.end());
    EXPECT_EQ(dup, addrs.end());

    for (void* p : ptrs) alloc.Deallocate(p);
}

TEST(SlabAllocator_SingleThreaded, DeallocateThenAllocateKeepsReturningValidPointers)
{
    constexpr std::size_t kBlockSize = 128;
    constexpr std::size_t kAlignment = 64;
    constexpr std::size_t kSlabBytes = 64 * 1024;

    lne::SlabAllocator alloc(kBlockSize, kAlignment, kSlabBytes);

    std::vector<void*> live;
    live.reserve(1000);

    for (int i = 0; i < 2000; ++i)
    {
        void* p = alloc.Allocate();
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(IsAlignedTo(reinterpret_cast<std::uintptr_t>(p), kAlignment));
        live.push_back(p);

        if ((i % 3) == 0)
        {
            alloc.Deallocate(live.back());
            live.pop_back();
        }
    }

    for (void* p : live)
        alloc.Deallocate(p);
}

#pragma endregion
}
