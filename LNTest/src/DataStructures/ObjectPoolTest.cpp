#include "pch.h"
#include "ObjectPoolTest.h"
#include "Engine/Core/DataStructures/ObjectPool.h"

static_assert(std::is_trivial_v<TrivialPod>);
static_assert(std::is_trivially_destructible_v<TrivialPod>);
static_assert(std::is_trivial_v<TrivialSmall>);
using namespace lne;

namespace ObjectPoolTests
{

// these tests were generated using AI since I'm lazy...
template <class ObjType>
struct PoolNodeLayout
{
    struct NodeMirror
    {
        NodeMirror* Next;
        NodeMirror* Prev;
        alignas(ObjType) std::uint8_t Allocation[sizeof(ObjType)];
    };

    static constexpr std::size_t Alignment = alignof(NodeMirror);

    static std::size_t BlockSize()
    {
        const std::size_t sz = sizeof(NodeMirror);
        const std::size_t a = alignof(NodeMirror);
        const std::size_t rem = sz % a;
        return rem == 0 ? sz : (sz + (a - rem));
    }
};

template <class PoolT>
static std::size_t LiveCount(const PoolT& pool)
{
    // GetLiveHead/Tail return Node*, but Node is a nested type: we only use it opaquely.
    // We can still walk Next because Node is a complete type in this TU (header included).
    std::size_t count = 0;
    auto* it = pool.GetLiveHead();
    while (it)
    {
        ++count;
        it = it->Next;
    }
    return count;
}

template <class PoolT>
static bool ValidateLiveListLinks(const PoolT& pool)
{
    auto* head = pool.GetLiveHead();
    auto* tail = pool.GetLiveTail();

    if (head == nullptr || tail == nullptr)
        return head == nullptr && tail == nullptr;

    if (head->Prev != nullptr) return false;
    if (tail->Next != nullptr) return false;

    // forward: Prev consistency + tail reachability
    {
        auto* it = head;
        decltype(head) prev = nullptr;
        while (it)
        {
            if (it->Prev != prev) return false;
            prev = it;
            if (it->Next == nullptr && it != tail) return false;
            it = it->Next;
        }
    }

    // backward: Next consistency + head reachability
    {
        auto* it = tail;
        decltype(tail) next = nullptr;
        while (it)
        {
            if (it->Next != next) return false;
            next = it;
            if (it->Prev == nullptr && it != head) return false;
            it = it->Prev;
        }
    }

    return true;
}

template <class PoolT>
static std::vector<lne::ObjectPoolHandle> AllocateN(PoolT& pool, int n)
{
    using namespace lne;
    std::vector<ObjectPoolHandle> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i)
        out.push_back(pool.Allocate());
    return out;
}

// Convenience: deallocate all handles
template <class PoolT>
static void DeallocateAll(PoolT& pool, const std::vector<lne::ObjectPoolHandle>& hs)
{
    for (auto h : hs) pool.Deallocate(h);
}

class ObjectPoolTest_Trivial : public ::testing::Test
{
};

TEST_F(ObjectPoolTest_Trivial, InternalAllocator_Basics_AllocateAccessDeallocate)
{
    ObjectPool<TrivialPod> pool(nullptr);

    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    ObjectPoolHandle h = pool.Allocate();
    ASSERT_NE(h, INVALID_OBJECT_POOL_HANDLE);

    auto* obj = pool.Access(h);
    ASSERT_NE(obj, nullptr);

    // Trivial object: Allocate() does NOT default-init (because trivially default constructible)
    // So we only write then read to verify memory is usable.
    obj->a = 42;
    obj->b = 3.5f;
    obj->c = 0xDEADBEEFCAFEBABEull;

    EXPECT_FALSE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 1u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    pool.Deallocate(h);

    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));
}

TEST_F(ObjectPoolTest_Trivial, InternalAllocator_AccessInvalidHandle_ReturnsNull)
{
    ObjectPool<TrivialPod> pool(nullptr);

    EXPECT_EQ(pool.Access(INVALID_OBJECT_POOL_HANDLE), nullptr);

    // Deallocate invalid should be no-op.
    pool.Deallocate(INVALID_OBJECT_POOL_HANDLE);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
}

TEST_F(ObjectPoolTest_Trivial, InternalAllocator_ClearEmptiesAndResetsList)
{
    ObjectPool<TrivialPod> pool(nullptr);

    auto handles = AllocateN(pool, 50);
    EXPECT_EQ(LiveCount(pool), 50u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    pool.Clear();

    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    // Clear again should be safe
    pool.Clear();
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
}

TEST_F(ObjectPoolTest_Trivial, InternalAllocator_AllocateMany_HandlesUniqueWhileLive)
{
    ObjectPool<TrivialSmall> pool(nullptr);

    constexpr int N = 200;
    auto handles = AllocateN(pool, N);

    std::set<ObjectPoolHandle> uniq(handles.begin(), handles.end());
    EXPECT_EQ(static_cast<int>(uniq.size()), N);

    EXPECT_EQ(LiveCount(pool), static_cast<std::size_t>(N));
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    DeallocateAll(pool, handles);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
}

TEST_F(ObjectPoolTest_Trivial, InternalAllocator_ReusesFreedBlocksEventually)
{
    ObjectPool<TrivialPod> pool(nullptr);

    auto h1 = pool.Allocate();
    auto h2 = pool.Allocate();
    auto h3 = pool.Allocate();

    pool.Deallocate(h2);

    // Next allocate might reuse h2 depending on allocator policy.
    // Slab allocator typically uses freed list first -> very likely reuse.
    auto h4 = pool.Allocate();

    // This is allocator-policy dependent. If your SlabAllocator always reuses freed blocks first,
    // this should hold. If not guaranteed, you can relax this check.
    EXPECT_TRUE(h4 == h2 || h4 == h1 || h4 == h3);

    pool.Deallocate(h1);
    pool.Deallocate(h3);
    pool.Deallocate(h4);

    EXPECT_TRUE(pool.IsEmpty());
}

TEST_F(ObjectPoolTest_Trivial, ExternalAllocator_Basics_AllocatorMatchesExpectedLayout)
{
    using T = TrivialPod;
    constexpr std::size_t expectedAlignment = PoolNodeLayout<T>::Alignment;
    const std::size_t expectedBlockSize = PoolNodeLayout<T>::BlockSize();

    SlabAllocator extAlloc(expectedBlockSize, expectedAlignment);

    ObjectPool<T> pool(&extAlloc);

    EXPECT_EQ(&pool.GetAllocator(), &extAlloc);
    EXPECT_FALSE(pool.OwnsAllocator());

    auto h = pool.Allocate();
    ASSERT_NE(h, INVALID_OBJECT_POOL_HANDLE);

    auto* obj = pool.Access(h);
    ASSERT_NE(obj, nullptr);

    obj->a = 7;
    obj->b = 1.25f;
    obj->c = 99;

    EXPECT_EQ(LiveCount(pool), 1u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    pool.Deallocate(h);

    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));
}

TEST_F(ObjectPoolTest_Trivial, ExternalAllocator_ClearAndReuseAcrossAllocations)
{
    using T = TrivialSmall;
    constexpr std::size_t expectedAlignment = PoolNodeLayout<T>::Alignment;
    const std::size_t expectedBlockSize = PoolNodeLayout<T>::BlockSize();

    SlabAllocator extAlloc(expectedBlockSize, expectedAlignment);
    ObjectPool<T> pool(&extAlloc);

    auto hs1 = AllocateN(pool, 100);
    EXPECT_EQ(LiveCount(pool), 100u);

    pool.Clear();
    EXPECT_TRUE(pool.IsEmpty());

    auto hs2 = AllocateN(pool, 100);
    EXPECT_EQ(LiveCount(pool), 100u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    DeallocateAll(pool, hs2);
    EXPECT_TRUE(pool.IsEmpty());

    // hs1 were "lost" handles after Clear(); that’s correct by design (Clear deallocates nodes).
    // Just ensure the pool’s bookkeeping is consistent.
    EXPECT_TRUE(ValidateLiveListLinks(pool));
}

TEST_F(ObjectPoolTest_Trivial, OneExternalAllocator_MultiplePools_SameObjType)
{
    using T = TrivialPod;
    constexpr std::size_t expectedAlignment = PoolNodeLayout<T>::Alignment;
    const std::size_t expectedBlockSize = PoolNodeLayout<T>::BlockSize();

    SlabAllocator sharedAlloc(expectedBlockSize, expectedAlignment);

    ObjectPool<T> poolA(&sharedAlloc);
    ObjectPool<T> poolB(&sharedAlloc);
    ObjectPool<T> poolC(&sharedAlloc);

    auto a = AllocateN(poolA, 30);
    auto b = AllocateN(poolB, 40);
    auto c = AllocateN(poolC, 50);

    EXPECT_EQ(LiveCount(poolA), 30u);
    EXPECT_EQ(LiveCount(poolB), 40u);
    EXPECT_EQ(LiveCount(poolC), 50u);

    EXPECT_TRUE(ValidateLiveListLinks(poolA));
    EXPECT_TRUE(ValidateLiveListLinks(poolB));
    EXPECT_TRUE(ValidateLiveListLinks(poolC));

    // Deallocate in different order (to stress unlink correctness)
    std::reverse(b.begin(), b.end());
    DeallocateAll(poolB, b);

    DeallocateAll(poolA, a);
    poolC.Clear(); // mix Deallocate and Clear

    EXPECT_TRUE(poolA.IsEmpty());
    EXPECT_TRUE(poolB.IsEmpty());
    EXPECT_TRUE(poolC.IsEmpty());

    EXPECT_TRUE(ValidateLiveListLinks(poolA));
    EXPECT_TRUE(ValidateLiveListLinks(poolB));
    EXPECT_TRUE(ValidateLiveListLinks(poolC));
}

TEST_F(ObjectPoolTest_Trivial, UnlinkCorrectness_RemoveHeadTailMiddle)
{
    ObjectPool<TrivialSmall> pool(nullptr);

    // Allocate 3: head, mid, tail
    auto h1 = pool.Allocate();
    auto h2 = pool.Allocate();
    auto h3 = pool.Allocate();

    ASSERT_EQ(LiveCount(pool), 3u);
    ASSERT_TRUE(ValidateLiveListLinks(pool));

    // Remove head
    pool.Deallocate(h1);
    EXPECT_EQ(LiveCount(pool), 2u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    // Remove tail (currently h3)
    pool.Deallocate(h3);
    EXPECT_EQ(LiveCount(pool), 1u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    // Remove remaining (middle originally)
    pool.Deallocate(h2);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));
}

TEST_F(ObjectPoolTest_Trivial, EmplaceWorksForTrivialToo)
{
    struct TrivialWithCtorLikeInit
    {
        int x;
        int y;
    };

    ObjectPool<TrivialWithCtorLikeInit> pool(nullptr);
    pool.Emplace();
}

// ------------------------------------------------------------
// Non-trivial fixture + non-trivial types
// ------------------------------------------------------------
class ObjectPoolTest_NotTrivial : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // ensure clean counters each test
        NonTrivialCounters::Reset();
        ThrowsOnCtorCounters::Reset();
    }

    struct NonTrivialCounters
    {
        static inline std::atomic<int> defaultCtor{ 0 };
        static inline std::atomic<int> argCtor{ 0 };
        static inline std::atomic<int> copyCtor{ 0 };
        static inline std::atomic<int> moveCtor{ 0 };
        static inline std::atomic<int> dtor{ 0 };

        static void Reset()
        {
            defaultCtor.store(0);
            argCtor.store(0);
            copyCtor.store(0);
            moveCtor.store(0);
            dtor.store(0);
        }
    };

    struct NonTrivial
    {
        int x = 0;
        int y = 0;

        NonTrivial()
        {
            ++NonTrivialCounters::defaultCtor;
        }

        explicit NonTrivial(int v)
            : x(v), y(v + 1)
        {
            ++NonTrivialCounters::argCtor;
        }

        NonTrivial(int a, int b)
            : x(a), y(b)
        {
            ++NonTrivialCounters::argCtor;
        }

        NonTrivial(const NonTrivial& other)
            : x(other.x), y(other.y)
        {
            ++NonTrivialCounters::copyCtor;
        }

        NonTrivial(NonTrivial&& other) noexcept
            : x(other.x), y(other.y)
        {
            other.x = 0;
            other.y = 0;
            ++NonTrivialCounters::moveCtor;
        }

        ~NonTrivial()
        {
            ++NonTrivialCounters::dtor;
        }
    };

    struct ThrowsOnCtorCounters
    {
        static inline std::atomic<int> ctor{ 0 };
        static inline std::atomic<int> dtor{ 0 };

        static void Reset()
        {
            ctor.store(0);
            dtor.store(0);
        }
    };

    // A type whose constructor throws. Used to test exception safety behavior.
    struct ThrowsOnCtor
    {
        ThrowsOnCtor()
        {
            ++ThrowsOnCtorCounters::ctor;
            throw std::runtime_error("boom");
        }
        ~ThrowsOnCtor()
        {
            ++ThrowsOnCtorCounters::dtor;
        }
    };
};

// ------------------------------------------------------------
// NotTrivial tests
// ------------------------------------------------------------

TEST_F(ObjectPoolTest_NotTrivial, InternalAllocator_AllocateCallsDefaultCtor_AndDeallocateCallsDtor)
{
    ObjectPool<NonTrivial> pool(nullptr);

    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 0);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 0);

    auto h1 = pool.Allocate();
    ASSERT_NE(h1, INVALID_OBJECT_POOL_HANDLE);
    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 1);
    EXPECT_EQ(LiveCount(pool), 1u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    auto* o1 = pool.Access(h1);
    ASSERT_NE(o1, nullptr);

    pool.Deallocate(h1);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 1);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));
}

TEST_F(ObjectPoolTest_NotTrivial, InternalAllocator_EmplaceCallsArgCtor_AndDeallocateCallsDtor)
{
    ObjectPool<NonTrivial> pool(nullptr);

    auto h = pool.Emplace(123);
    ASSERT_NE(h, INVALID_OBJECT_POOL_HANDLE);

    EXPECT_EQ(NonTrivialCounters::argCtor.load(), 1);
    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 0);

    auto* o = pool.Access(h);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->x, 123);
    EXPECT_EQ(o->y, 124);

    pool.Deallocate(h);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 1);
    EXPECT_TRUE(pool.IsEmpty());
}

TEST_F(ObjectPoolTest_NotTrivial, InternalAllocator_EmplacePerfectForwarding_MoveOnlyArg)
{
    TakesMoveOnly::ctor.store(0);
    TakesMoveOnly::dtor.store(0);

    ObjectPool<TakesMoveOnly> pool(nullptr);

    auto h = pool.Emplace(MoveOnly{ 77 });
    ASSERT_NE(h, INVALID_OBJECT_POOL_HANDLE);
    EXPECT_EQ(TakesMoveOnly::ctor.load(), 1);

    auto* o = pool.Access(h);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->m.v, 77);

    pool.Deallocate(h);
    EXPECT_EQ(TakesMoveOnly::dtor.load(), 1);
    EXPECT_TRUE(pool.IsEmpty());
}

TEST_F(ObjectPoolTest_NotTrivial, InternalAllocator_ClearCallsDtorForAllLive)
{
    ObjectPool<NonTrivial> pool(nullptr);

    constexpr int N = 64;
    auto hs = AllocateN(pool, N);

    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), N);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 0);
    EXPECT_EQ(LiveCount(pool), static_cast<std::size_t>(N));
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    pool.Clear();

    EXPECT_EQ(NonTrivialCounters::dtor.load(), N);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(LiveCount(pool), 0u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    // Idempotent
    pool.Clear();
    EXPECT_EQ(NonTrivialCounters::dtor.load(), N);
}

TEST_F(ObjectPoolTest_NotTrivial, InternalAllocator_MixedDeallocateAndClear_DtorCountsMatchExactly)
{
    ObjectPool<NonTrivial> pool(nullptr);

    auto h1 = pool.Allocate();
    auto h2 = pool.Allocate();
    auto h3 = pool.Allocate();
    auto h4 = pool.Allocate();

    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 4);
    EXPECT_EQ(LiveCount(pool), 4u);

    pool.Deallocate(h2);
    pool.Deallocate(h4);

    EXPECT_EQ(NonTrivialCounters::dtor.load(), 2);
    EXPECT_EQ(LiveCount(pool), 2u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    pool.Clear();
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 4);
    EXPECT_TRUE(pool.IsEmpty());
}

TEST_F(ObjectPoolTest_NotTrivial, ExternalAllocator_AllocateEmplaceDestroy_Basics)
{
    using T = NonTrivial;
    constexpr std::size_t expectedAlignment = PoolNodeLayout<T>::Alignment;
    const std::size_t expectedBlockSize = PoolNodeLayout<T>::BlockSize();

    SlabAllocator extAlloc(expectedBlockSize, expectedAlignment);
    ObjectPool<T> pool(&extAlloc);

    EXPECT_FALSE(pool.OwnsAllocator());
    EXPECT_EQ(&pool.GetAllocator(), &extAlloc);

    auto h1 = pool.Allocate();
    auto h2 = pool.Emplace(9, 10);

    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 1);
    EXPECT_EQ(NonTrivialCounters::argCtor.load(), 1);
    EXPECT_EQ(LiveCount(pool), 2u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    auto* o2 = pool.Access(h2);
    ASSERT_NE(o2, nullptr);
    EXPECT_EQ(o2->x, 9);
    EXPECT_EQ(o2->y, 10);

    pool.Deallocate(h1);
    pool.Deallocate(h2);

    EXPECT_EQ(NonTrivialCounters::dtor.load(), 2);
    EXPECT_TRUE(pool.IsEmpty());
}

TEST_F(ObjectPoolTest_NotTrivial, OneExternalAllocator_MultiplePools_SameObjType_DtorsCorrectPerPool)
{
    using T = NonTrivial;
    constexpr std::size_t expectedAlignment = PoolNodeLayout<T>::Alignment;
    const std::size_t expectedBlockSize = PoolNodeLayout<T>::BlockSize();

    SlabAllocator sharedAlloc(expectedBlockSize, expectedAlignment);

    ObjectPool<T> poolA(&sharedAlloc);
    ObjectPool<T> poolB(&sharedAlloc);

    auto a = AllocateN(poolA, 10); // 10 default ctors
    auto b = AllocateN(poolB, 20); // 20 default ctors

    EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 30);
    EXPECT_EQ(LiveCount(poolA), 10u);
    EXPECT_EQ(LiveCount(poolB), 20u);

    // Clear A, Deallocate all B
    poolA.Clear();
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 10);
    EXPECT_TRUE(poolA.IsEmpty());
    EXPECT_EQ(LiveCount(poolA), 0u);

    DeallocateAll(poolB, b);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 30);
    EXPECT_TRUE(poolB.IsEmpty());

    EXPECT_TRUE(ValidateLiveListLinks(poolA));
    EXPECT_TRUE(ValidateLiveListLinks(poolB));
}

TEST_F(ObjectPoolTest_NotTrivial, DestructorOfPool_CallsClearForNonTrivialObjects)
{
    NonTrivialCounters::Reset();

    {
        ObjectPool<NonTrivial> pool(nullptr);
        auto hs = AllocateN(pool, 15);
        (void)hs;

        EXPECT_EQ(NonTrivialCounters::defaultCtor.load(), 15);
        EXPECT_EQ(NonTrivialCounters::dtor.load(), 0);
        EXPECT_FALSE(pool.IsEmpty());
    }

    // pool destroyed => Clear() executed in destructor => all dtors called
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 15);
}

TEST_F(ObjectPoolTest_NotTrivial, DeallocateInvalidHandle_IsNoOp)
{
    ObjectPool<NonTrivial> pool(nullptr);

    pool.Deallocate(INVALID_OBJECT_POOL_HANDLE);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 0);

    auto h = pool.Allocate();
    pool.Deallocate(h);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 1);

    pool.Deallocate(INVALID_OBJECT_POOL_HANDLE);
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 1);
}

TEST_F(ObjectPoolTest_NotTrivial, UnlinkCorrectness_RemoveHeadTailMiddle)
{
    ObjectPool<NonTrivial> pool(nullptr);

    auto h1 = pool.Allocate();
    auto h2 = pool.Allocate();
    auto h3 = pool.Allocate();

    ASSERT_EQ(LiveCount(pool), 3u);
    ASSERT_TRUE(ValidateLiveListLinks(pool));

    // remove head
    pool.Deallocate(h1);
    EXPECT_EQ(LiveCount(pool), 2u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    // remove tail (h3)
    pool.Deallocate(h3);
    EXPECT_EQ(LiveCount(pool), 1u);
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    // remove remaining middle
    pool.Deallocate(h2);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    EXPECT_EQ(NonTrivialCounters::dtor.load(), 3);
}

TEST_F(ObjectPoolTest_NotTrivial, HandlesUniqueWhileLive)
{
    ObjectPool<NonTrivial> pool(nullptr);

    constexpr int N = 100;
    auto hs = AllocateN(pool, N);

    std::set<ObjectPoolHandle> uniq(hs.begin(), hs.end());
    EXPECT_EQ(static_cast<int>(uniq.size()), N);
    EXPECT_EQ(LiveCount(pool), static_cast<std::size_t>(N));
    EXPECT_TRUE(ValidateLiveListLinks(pool));

    DeallocateAll(pool, hs);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(NonTrivialCounters::dtor.load(), N);
}

TEST_F(ObjectPoolTest_NotTrivial, AccessReturnsStablePointerUntilDeallocate)
{
    ObjectPool<NonTrivial> pool(nullptr);

    auto h = pool.Emplace(11, 22);
    auto* p1 = pool.Access(h);
    ASSERT_NE(p1, nullptr);

    p1->x = 99;
    auto* p2 = pool.Access(h);
    ASSERT_NE(p2, nullptr);

    EXPECT_EQ(p1, p2);
    EXPECT_EQ(p2->x, 99);

    pool.Deallocate(h);
    EXPECT_TRUE(pool.IsEmpty());
    EXPECT_EQ(NonTrivialCounters::dtor.load(), 1);
}

// ------------------------------------------------------------
// Exception-safety tests (IMPORTANT)
// ------------------------------------------------------------

TEST_F(ObjectPoolTest_NotTrivial, Allocate_WhenCtorThrows_LeavesPoolConsistent)
{
    // This test is here to *detect* the classic issue:
    // LinkLive(node) happens BEFORE placement-new. If ctor throws,
    // node remains in the live list => Clear()/dtor may treat it as constructed => UB.
    //
    // If your pool is not exception-safe (and exceptions are enabled), this test may fail/crash.
    // That’s useful: it tells you what to fix.

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    ObjectPool<ThrowsOnCtor> pool(nullptr);

    EXPECT_TRUE(pool.IsEmpty());

    try
    {
        (void)pool.Allocate();
        FAIL() << "Expected constructor to throw.";
    }
    catch (const std::runtime_error&)
    {
        // ok
    }
    catch (...)
    {
        FAIL() << "Unexpected exception type.";
    }

    // What *should* be true for strong safety: pool still empty and links valid.
    // If this fails, your current implementation likely leaked a live node.
    EXPECT_TRUE(ValidateLiveListLinks(pool));
    EXPECT_TRUE(pool.IsEmpty()) << "Pool contains a live node after throwing ctor; "
        "LinkLive() likely happens before successful construction.";

    // Even if not empty, Clear() must not crash (but it may if it tries to destroy an unconstructed object).
    // We'll keep this as an additional guard.
    EXPECT_NO_THROW(pool.Clear());
#else
    GTEST_SKIP() << "Exceptions are disabled; cannot test throwing constructor behavior.";
#endif
}

TEST_F(ObjectPoolTest_NotTrivial, Emplace_WhenCtorThrows_LeavesPoolConsistent)
{
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    ObjectPool<ThrowsOnCtor> pool(nullptr);

    try
    {
        (void)pool.Emplace();
        FAIL() << "Expected constructor to throw.";
    }
    catch (const std::runtime_error&)
    {
        // ok
    }
    catch (...)
    {
        FAIL() << "Unexpected exception type.";
    }

    EXPECT_TRUE(ValidateLiveListLinks(pool));
    EXPECT_TRUE(pool.IsEmpty()) << "Pool contains a live node after throwing ctor.";

    EXPECT_NO_THROW(pool.Clear());
#else
    GTEST_SKIP() << "Exceptions are disabled; cannot test throwing constructor behavior.";
#endif
}
