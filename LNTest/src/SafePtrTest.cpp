#include "SafePtrTest.h"

namespace SafePtrTests
{
using namespace lne;
TEST(SafePtr_SingleThread, DefaultCtor_YieldsNullPtr)
{
    SafePtr<DummyObject> ptr;
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.GetPtr(), nullptr);
}

TEST(SafePtr_SingleThread, NullptrCtor_YieldsNullPtr)
{
    SafePtr<DummyObject> ptr(nullptr);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.GetPtr(), nullptr);
}

TEST(SafePtr_SingleThread, ConstructFromRawPointer_IncrementsRefAndTracksLifetime)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> ptr(new DummyObject(123));

        EXPECT_TRUE(ptr);
        EXPECT_EQ(DummyObject::AliveCount, 1);

        // RefCountBase starts at 0 and Capture() is called once in this ctor. :contentReference[oaicite:1]{index=1}
        EXPECT_EQ(ptr->GetCount(), 1u);
        EXPECT_EQ(ptr->Value, 123);
    }

    // After leaving scope, ref count should drop to 0 and the object should be deleted.
    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, CopyConstructor_IncrementsRefCount)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> p1(new DummyObject());
        EXPECT_EQ(DummyObject::AliveCount, 1);
        EXPECT_EQ(p1->GetCount(), 1u);

        {
            SafePtr<DummyObject> p2(p1);
            EXPECT_EQ(DummyObject::AliveCount, 1);

            // Both point to the same object; ref count should be 2.
            EXPECT_EQ(p1->GetCount(), 2u);
            EXPECT_EQ(p2->GetCount(), 2u);
            EXPECT_EQ(p1.GetPtr(), p2.GetPtr());
        }

        // p2 destroyed, ref count back to 1
        EXPECT_EQ(p1->GetCount(), 1u);
        EXPECT_EQ(DummyObject::AliveCount, 1);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, MoveConstructor_TransfersOwnershipWithoutChangingRefCount)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> p1(new DummyObject());
        EXPECT_EQ(p1->GetCount(), 1u);
        EXPECT_EQ(DummyObject::AliveCount, 1);

        SafePtr<DummyObject> p2(std::move(p1));

        EXPECT_FALSE(p1);
        EXPECT_TRUE(p2);
        EXPECT_EQ(DummyObject::AliveCount, 1);
        EXPECT_EQ(p2->GetCount(), 1u);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, CopyAssignment_UpdatesCountsAndDeletesOldTarget)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> a(new DummyObject(1));
        SafePtr<DummyObject> b(new DummyObject(2));

        EXPECT_EQ(DummyObject::AliveCount, 2);
        EXPECT_EQ(a->GetCount(), 1u);
        EXPECT_EQ(b->GetCount(), 1u);

        // Assign b into a: a's existing pointee should be released (and deleted),
        // then both a and b point to the same object.
        a = b;

        EXPECT_EQ(DummyObject::AliveCount, 1); // old object deleted
        EXPECT_EQ(a.GetPtr(), b.GetPtr());
        EXPECT_EQ(a->GetCount(), 2u);
        EXPECT_EQ(b->GetCount(), 2u);
        EXPECT_EQ(a->Value, 2);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, MoveAssignment_TransfersOwnershipAndDeletesOldTarget)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> a(new DummyObject(1));
        SafePtr<DummyObject> b(new DummyObject(2));

        EXPECT_EQ(DummyObject::AliveCount, 2);
        EXPECT_EQ(a->GetCount(), 1u);
        EXPECT_EQ(b->GetCount(), 1u);

        a = std::move(b);

        EXPECT_FALSE(b);
        EXPECT_TRUE(a);
        EXPECT_EQ(DummyObject::AliveCount, 1);
        EXPECT_EQ(a->GetCount(), 1u);
        EXPECT_EQ(a->Value, 2);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, ResetWithoutArg_ReleasesAndDeletes)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> ptr(new DummyObject());
        EXPECT_EQ(DummyObject::AliveCount, 1);
        EXPECT_EQ(ptr->GetCount(), 1u);

        ptr.Reset();

        EXPECT_FALSE(ptr);
        EXPECT_EQ(DummyObject::AliveCount, 0);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, ResetToNullptr_ReleasesAndDeletes)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> ptr(new DummyObject());
        EXPECT_EQ(DummyObject::AliveCount, 1);

        ptr.Reset(nullptr);

        EXPECT_FALSE(ptr);
        EXPECT_EQ(DummyObject::AliveCount, 0);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, ResetToNewPointer_UpdatesCounts)
{
    DummyObject::AliveCount = 0;

    {
        SafePtr<DummyObject> ptr(new DummyObject(1));
        EXPECT_EQ(DummyObject::AliveCount, 1);
        EXPECT_EQ(ptr->GetCount(), 1u);

        DummyObject* raw = new DummyObject(2); // aliveCount becomes 2
        ptr.Reset(raw);

        // Old object deleted, new object has count 1
        EXPECT_EQ(DummyObject::AliveCount, 1);
        EXPECT_EQ(ptr->GetCount(), 1u);
        EXPECT_EQ(ptr->Value, 2);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, Operators_DereferenceAndBooleanAndEquality)
{
    DummyObject::AliveCount = 0;

    {
        DummyObject* raw = new DummyObject(10);
        SafePtr<DummyObject> p1(raw);
        SafePtr<DummyObject> p2(p1);

        EXPECT_TRUE(p1);
        EXPECT_TRUE(p2);

        EXPECT_EQ((*p1).Value, 10);
        EXPECT_EQ(p1->Value, 10);

        EXPECT_TRUE(p1 == p2);
        EXPECT_TRUE(p1 == raw);
    }

    EXPECT_EQ(DummyObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, CrossTypeCopyConstructor_FromDerivedToBase)
{
    BaseObject::AliveCount = 0;

    {
        SafePtr<DerivedObject> derived(new DerivedObject());
        EXPECT_EQ(BaseObject::AliveCount, 1);
        EXPECT_EQ(derived->GetCount(), 1u);

        // Use templated ctor: SafePtr<BaseObject> from SafePtr<DerivedObject> :contentReference[oaicite:2]{index=2}
        SafePtr<BaseObject> base(derived);

        EXPECT_EQ(BaseObject::AliveCount, 1);
        EXPECT_EQ(base.GetPtr(), static_cast<BaseObject*>(derived.GetPtr()));

        EXPECT_EQ(base->GetCount(), 2u);
        EXPECT_EQ(derived->GetCount(), 2u);
    }

    EXPECT_EQ(BaseObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, CrossTypeMoveConstructor_FromDerivedToBase)
{
    BaseObject::AliveCount = 0;

    {
        SafePtr<DerivedObject> derived(new DerivedObject());
        EXPECT_EQ(BaseObject::AliveCount, 1);

        SafePtr<BaseObject> base(std::move(derived));

        EXPECT_FALSE(derived);
        EXPECT_TRUE(base);
        EXPECT_EQ(BaseObject::AliveCount, 1);
        EXPECT_EQ(base->GetCount(), 1u);
    }

    EXPECT_EQ(BaseObject::AliveCount, 0);
}

TEST(SafePtr_SingleThread, GetAs_DowncastBaseToDerived)
{
    BaseObject::AliveCount = 0;

    {
        // Store DerivedObject in a SafePtr<BaseObject>
        SafePtr<BaseObject> base(new DerivedObject());
        EXPECT_EQ(BaseObject::AliveCount, 1);
        EXPECT_EQ(base->GetCount(), 1u);

        // GetAs<DerivedObject> uses the templated converting ctor internally.
        SafePtr<DerivedObject> derived = base.GetAs<DerivedObject>();

        EXPECT_EQ(BaseObject::AliveCount, 1);
        EXPECT_EQ(base->GetCount(), 2u);
        EXPECT_EQ(derived->GetCount(), 2u);

        EXPECT_NE(derived.GetPtr(), nullptr);
        EXPECT_EQ(static_cast<BaseObject*>(derived.GetPtr()), base.GetPtr());
    }

    EXPECT_EQ(BaseObject::AliveCount, 0);
}
}

