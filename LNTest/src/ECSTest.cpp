#include "ECSTest.h"
#include "Engine/ECS/ecs.h"

namespace ECSTest
{
using namespace lne;

TEST(TypeTests, ComponentTypeTest)
{
    ComponentTypeID transformType = ComponentType<Transform>();
    ComponentTypeID aType = ComponentType<A>();
    ComponentTypeID bType = ComponentType<B>();
    EXPECT_EQ(aType, ComponentType<A>());
    EXPECT_EQ(bType, ComponentType<B>());
    EXPECT_EQ(transformType, ComponentType<Transform>());
}

////////////////////////////////////////////////////////////////////////////////////////
// CircularBuffer Tests ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

TEST(CircularBufferTests, CreateCircularBuffer)
{
    CircularBuffer<int> buffer;

    EXPECT_EQ(buffer.GetCapacity(), 16);

    CircularBuffer<int> customBuffer(10);
    EXPECT_EQ(customBuffer.GetCapacity(), 10);
}

TEST(CircularBufferTests, CircularBufferCount)
{
    CircularBuffer<int> buffer(3);

    EXPECT_EQ(buffer.GetSize(), 0);
    EXPECT_NE(buffer.IsEmpty(), false);
    EXPECT_EQ(buffer.IsEmpty(), true);
    EXPECT_EQ(buffer.IsFull(), false);

    buffer.PushBack(1);
    buffer.PushBack(2);
    buffer.PushBack(3);

    EXPECT_EQ(buffer.GetSize(), 3);
    EXPECT_EQ(buffer.IsEmpty(), false);
    EXPECT_EQ(buffer.IsFull(), true);
}

TEST(CircularBufferTests, CircularBufferCreationStressTest)
{
    CircularBuffer<int> buffer(10000);

    EXPECT_EQ(buffer.GetCapacity(), 10000);
    EXPECT_EQ(buffer.GetSize(), 0);
    EXPECT_EQ(buffer.IsEmpty(), true);

    for (int i = 0; i < 10000; i++)
    {
        buffer.PushBack(i);
    }

    EXPECT_EQ(buffer.GetCapacity(), 10000);
    EXPECT_EQ(buffer.GetSize(), 10000);
    EXPECT_EQ(buffer.IsEmpty(), false);
}

TEST(CircularBufferTests, CircularBufferAccessors)
{
    CircularBuffer<int> buffer(3);

    buffer.PushBack(1);
    buffer.PushBack(2);
    buffer.PushBack(3);

    EXPECT_EQ(buffer.GetFront(), 1);
    EXPECT_EQ(buffer.GetBack(), 3);
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 3);
}

TEST(CircularBufferTests, CircularBufferResize)
{
    CircularBuffer<int> buffer(3);
    EXPECT_EQ(buffer.GetCapacity(), 3);
    for (int i = 0; i < 3; ++i)
        buffer.PushBack(i);
    EXPECT_EQ(buffer.GetSize(), 3);
    buffer.Resize(6);
    EXPECT_EQ(buffer.GetCapacity(), 6);
    for (int i = 0; i < 3; ++i)
    {
        auto num = buffer.PopFront();
        EXPECT_EQ(num, i);
    }
    EXPECT_EQ(buffer.GetSize(), 0);
    for (int i = 0; i < 6; ++i)
        buffer.PushFront(i);
    EXPECT_EQ(buffer.GetSize(), 6);
    EXPECT_EQ(buffer.GetCapacity(), 6);
    buffer.PushBack(6);
    EXPECT_EQ(buffer.GetCapacity(), uint32_t(6 * 2));
    EXPECT_EQ(buffer.GetSize(), 7);
    buffer.Resize(7U);
    EXPECT_EQ(buffer.GetCapacity(), 7U);
    auto num = buffer.PopBack();
    EXPECT_EQ(num, 6);
    EXPECT_EQ(buffer.GetSize(), 6);
    for (int i = 0; i < 6; ++i)
    {
        auto num = buffer.PopBack();
        EXPECT_EQ(num, i);
    }
    EXPECT_EQ(buffer.GetSize(), 0);
    EXPECT_EQ(buffer.GetCapacity(), 7U);
    buffer.Resize(30);
    EXPECT_EQ(buffer.GetCapacity(), 30);
    EXPECT_EQ(buffer.GetSize(), 0);
}

TEST(CircularBufferTests, CircularBufferComplexStructTest)
{
    CircularBuffer<ComplexStruct> buffer(3);

    EXPECT_EQ(buffer.GetCapacity(), 3);
    EXPECT_EQ(buffer.GetSize(), 0);
    EXPECT_EQ(buffer.IsEmpty(), true);

    for (int i = 0; i < 5; ++i)
        buffer.PushBack(6, new char[6] { 'h', 'e', 'l', 'l', 'o', '\0' });

    EXPECT_EQ(buffer.GetSize(), 5);

    CircularBuffer<ComplexStruct> buffer2 = buffer;
    EXPECT_EQ(buffer2.GetSize(), 5);
    EXPECT_EQ(buffer2.GetCapacity(), buffer.GetCapacity());
    for (int i = 0; i < 2; ++i)
    {
        auto complexStruct = buffer2.PopFront();
        EXPECT_EQ(complexStruct.num, 6);
        EXPECT_STRCASEEQ("hello", complexStruct.characters);
    }
    EXPECT_EQ(buffer2.GetSize(), 3);
    for (int i = 0; i < 2; ++i)
        buffer2.PushBack(ComplexStruct(6, new char[6] { 'w', 'o', 'r', 'l', 'd', '\0' }));

    CircularBuffer<ComplexStruct> buffer3 = buffer2;
    EXPECT_EQ(buffer3.GetSize(), 5);
    EXPECT_EQ(buffer3.GetCapacity(), buffer2.GetCapacity());
    for (int i = 0; i < 5; ++i)
    {
        auto complexStruct = buffer3.PopFront();
        EXPECT_EQ(complexStruct.num, 6);
        if (i < 3)
        {
            EXPECT_STRCASEEQ("hello", complexStruct.characters);
            continue;
        }
        EXPECT_STRCASEEQ("world", complexStruct.characters);
    }

    for (auto& complexStruct : buffer)
    {
        EXPECT_EQ(complexStruct.num, 6);
        EXPECT_STRCASEEQ("hello", complexStruct.characters);
        buffer.PopFront();
    }
    EXPECT_EQ(buffer.GetSize(), 0);
    EXPECT_EQ(buffer2.GetSize(), 5);
    EXPECT_EQ(buffer3.GetSize(), 0);
    EXPECT_EQ(buffer2.GetCapacity(), buffer.GetCapacity());
    EXPECT_EQ(buffer3.GetCapacity(), buffer2.GetCapacity());
}

#ifdef CIRCULAR_BUFFER_STRESS_TEST
TEST(CircularBufferTests, CircularBufferStressResize)
{
    CircularBuffer<uint8_t> buffer(uint32_t(std::numeric_limits<uint32_t>::max() / 2));
    EXPECT_EQ(buffer.GetCapacity(), uint32_t(std::numeric_limits<uint32_t>::max() / 2));
    for (uint32_t i = 0; i < uint32_t(std::numeric_limits<uint32_t>::max() / 2); ++i)
        buffer.PushBack(uint8_t(i) % std::numeric_limits<uint8_t>::max());
    EXPECT_EQ(buffer.GetSize(), uint32_t(std::numeric_limits<uint32_t>::max() / 2));
    EXPECT_EQ(buffer.GetCapacity(), uint32_t((std::numeric_limits<uint32_t>::max() / 2)));
    buffer.PushBack(1);
    EXPECT_EQ(buffer.GetCapacity(), uint32_t((std::numeric_limits<uint32_t>::max() / 2) * 2));
}
#endif // CIRCULAR_BUFFER_STRESS_TEST

////////////////////////////////////////////////////////////////////////////////////////
// Archetype Tests /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

class ArchetypeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        archetype = new Archetype();
    }

    void TearDown() override
    {
        delete archetype;
    }

    Archetype* archetype{ nullptr };
};

TEST_F(ArchetypeTest, AddEntityTest)
{
    EntityID e1 = 1;
    archetype->AddEntity(e1);

    const auto& entities = archetype->GetEntities();
    ASSERT_EQ(entities.size(), 1);
    EXPECT_EQ(entities[0], e1);
}

TEST_F(ArchetypeTest, RemoveEntityTest)
{
    EntityID e1 = 1;
    EntityID e2 = 2;
    EntityID e3 = 3;

    archetype->AddEntity(e1);
    archetype->AddEntity(e2);
    archetype->AddEntity(e3);
    archetype->RemoveEntity(e2);

    const auto& entities = archetype->GetEntities();
    ASSERT_EQ(entities.size(), 2);
    EXPECT_EQ(entities[0], e1);
    EXPECT_NE(entities[1], e2);
    EXPECT_EQ(entities[1], e3);
    archetype->AddEntity(e2);
    ASSERT_EQ(entities.size(), 3);
    EXPECT_EQ(entities[2], e2);
    archetype->RemoveEntity(e1);
    ASSERT_EQ(entities.size(), 2);
    EXPECT_NE(entities[0], e1);
    EXPECT_EQ(entities[0], e2);

    EXPECT_FALSE(archetype->HasEntity(e1));
    EXPECT_TRUE(archetype->HasEntity(e2));
    EXPECT_TRUE(archetype->HasEntity(e3));
}

TEST_F(ArchetypeTest, CreateAndAddComponentTest)
{
    EntityID e1 = 1;
    archetype->CreateComponentStorages<Transform, A>();

    archetype->AddEntity(e1);
    archetype->AddComponents<Transform, A>(e1, Transform({ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }), A(42));

    auto& transformStorage = archetype->GetComponentStorage<Transform>();
    auto& aStorage = archetype->GetComponentStorage<A>();

    ASSERT_EQ(transformStorage.Components.size(), 1);
    ASSERT_EQ(aStorage.Components.size(), 1);

    EXPECT_EQ(transformStorage.Components[0], Transform({ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }));
    EXPECT_EQ(aStorage.Components[0], A(42));
}

TEST_F(ArchetypeTest, RemoveComponentTest)
{
    EntityID e1 = 1;
    archetype->CreateComponentStorages<Transform, A>();

    archetype->AddEntity(e1);
    archetype->AddComponents<Transform, A>(e1, Transform({ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }), A(42));

    archetype->RemoveComponent<Transform>(e1);

    auto& aStorage = archetype->GetComponentStorage<A>();
    ASSERT_EQ(aStorage.Components.size(), 1);
    EXPECT_EQ(aStorage.Components[0], A(42));
}

TEST_F(ArchetypeTest, RemoveAllComponentsTest)
{
    EntityID e1 = 1;
    EntityID e2 = 2;
    EntityID e3 = 3;

    archetype->CreateComponentStorages<Transform, A, B>();

    archetype->AddEntity(e1);
    archetype->AddEntity(e2);
    archetype->AddEntity(e3);

    archetype->AddComponents<Transform, A, B>(e1, Transform({ 1, 1, 1 }, { 0, 0, 0 }, { 1, 1, 1 }), A(10), B{ "Entity1" });
    archetype->AddComponents<Transform, A, B>(e2, Transform({ 2, 2, 2 }, { 0, 0, 0 }, { 1, 1, 1 }), A(20), B{ "Entity2" });
    archetype->AddComponents<Transform, A, B>(e3, Transform({ 3, 3, 3 }, { 0, 0, 0 }, { 1, 1, 1 }), A(30), B{ "Entity3" });

    // Remove components for the second entity (e2)
    archetype->RemoveComponents<Transform, A, B>(e2);

    auto& transformStorage = archetype->GetComponentStorage<Transform>();
    auto& aStorage = archetype->GetComponentStorage<A>();
    auto& bStorage = archetype->GetComponentStorage<B>();

    ASSERT_EQ(transformStorage.Components.size(), 2);
    ASSERT_EQ(aStorage.Components.size(), 2);
    ASSERT_EQ(bStorage.Components.size(), 2);

    EXPECT_EQ(transformStorage.Components[0], Transform({ 1, 1, 1 }, { 0, 0, 0 }, { 1, 1, 1 }));
    EXPECT_EQ(transformStorage.Components[1], Transform({ 3, 3, 3 }, { 0, 0, 0 }, { 1, 1, 1 }));

    EXPECT_EQ(aStorage.Components[0], A(10));
    EXPECT_EQ(aStorage.Components[1], A(30));

    EXPECT_EQ(bStorage.Components[0], B{ "Entity1" });
    EXPECT_EQ(bStorage.Components[1], B{ "Entity3" });
}

TEST_F(ArchetypeTest, HandleInvalidEntityRemoval)
{
    EntityID invalidEntity = 999;

    // Ensure no crash occurs when removing a non-existent entity
    EXPECT_NO_THROW(archetype->RemoveEntity(invalidEntity));
}

TEST_F(ArchetypeTest, HandleInvalidComponentRemoval)
{
    EntityID e1 = 1;
    archetype->CreateComponentStorages<Transform>();
    archetype->AddEntity(e1);
    archetype->AddComponents<Transform>(e1, Transform({ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }));

    // Ensure no crash occurs when removing a component that doesn't exist
    EXPECT_NO_THROW(archetype->RemoveComponent<A>(e1));
}

////////////////////////////////////////////////////////////////////////////////////////
// EntityRegistry Tests ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
class EntityRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EntityRegistry::RegisterComponentTypes<Transform, A, B>();
    }
};

TEST_F(EntityRegistryTest, CreateEntity)
{
    EntityRegistry registry;

    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();

    EXPECT_EQ(entity0, 0);
    EXPECT_EQ(entity1, 1);
    EXPECT_EQ(entity2, 2);
}

// Unit test to check if all the functions in the EntityRegistry class work as intended
TEST_F(EntityRegistryTest, GetComponent)
{
    EntityRegistry registry;

    Transform transform;
    A a;
    B b;
    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();
    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity1, a);
    registry.TryAddComponent(entity2, b);
    EXPECT_EQ(registry.GetComponent<Transform>(entity0), transform);
    EXPECT_EQ(registry.GetComponent<A>(entity1), a);
    EXPECT_EQ(registry.GetComponent<B>(entity2), b);
}

TEST_F(EntityRegistryTest, HasComponent)
{
    EntityRegistry registry;

    Transform transform;
    A a;
    B b;
    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();
    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity1, a);
    registry.EmplaceComponent<B>(entity2);
    EXPECT_TRUE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<A>(entity1));
    EXPECT_TRUE(registry.HasComponent<B>(entity2));
}

TEST_F(EntityRegistryTest, DeleteComponent)
{
    EntityRegistry registry;

    Transform transform;
    A a;
    B b;
    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();
    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity1, a);
    registry.TryAddComponent(entity2, b);
    registry.TryAddComponent(entity0, a);
    registry.TryAddComponent(entity1, b);
    registry.TryAddComponent(entity2, transform);
    EXPECT_TRUE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<A>(entity1));
    EXPECT_TRUE(registry.HasComponent<B>(entity2));
    bool r = registry.HasComponents<Transform, B>(entity2);
    EXPECT_TRUE(r);
    registry.DeleteComponent<Transform>(entity0);
    registry.DeleteComponent<A>(entity1);
    registry.DeleteComponent<B>(entity2);
    EXPECT_TRUE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<A>(entity1));
    EXPECT_TRUE(registry.HasComponent<B>(entity2));
    registry.Flush();
    EXPECT_FALSE(registry.HasComponent<Transform>(entity0));
    EXPECT_FALSE(registry.HasComponent<A>(entity1));
    EXPECT_FALSE(registry.HasComponent<B>(entity2));
}

TEST_F(EntityRegistryTest, DeleteEntity)
{
    EntityRegistry registry;

    Transform transform;
    A a;
    B b;
    EntityID entity0 = registry.CreateEntity();
    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity0, b);
    EXPECT_TRUE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<B>(entity0));
    registry.DeleteEntity(entity0);
    EXPECT_TRUE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<B>(entity0));
    registry.Flush();
    EXPECT_FALSE(registry.HasComponent<Transform>(entity0));
    EXPECT_FALSE(registry.HasComponent<B>(entity0));
    registry.DeleteEntity(entity0);
    registry.Flush();
}

TEST_F(EntityRegistryTest, EntityStressTest)
{
    EntityRegistry registry;
    {
        SCOPED_TRACE("Create entities");
        for (int i = 0; i < 8192; i++)
        {
            try
            {
                EntityID entity = registry.CreateEntity();
                Transform transform{ {(float)-i, (float)i, (float)i * 10}, {0, 0, 0}, {1, 1, 1} };
                A a{ i };
                B b{ "Entity" + std::to_string(i) };
                registry.TryAddComponent(entity, transform);
                registry.TryAddComponent(entity, a);
                registry.TryAddComponent(entity, b);
                bool r = registry.HasComponents<Transform, A, B>(entity);
                EXPECT_TRUE(r);
            }
            catch (const std::exception& e)
            {
                FAIL() << e.what();
                break;
            }
        }
    }
    {
        SCOPED_TRACE("Delete Components");
        for (EntityID entity = 100; entity < 420; entity++)
        {
            registry.DeleteComponent<Transform>(entity);
        }
        for (EntityID entity = 420; entity < 1000; entity++)
        {
            registry.DeleteComponent<A>(entity);
        }
        for (EntityID entity = 1000; entity < 1400; entity++)
        {
            registry.DeleteComponent<B>(entity);
        }
        for (EntityID entity = 1400; entity < 1800; entity++)
        {
            registry.DeleteComponent<Transform>(entity);
            registry.DeleteComponent<B>(entity);
        }
        {
            SCOPED_TRACE("Flush");
            registry.Flush();
        }
        for (EntityID entity = 100; entity < 420; entity++)
        {
            EXPECT_FALSE(registry.HasComponent<Transform>(entity));
            EXPECT_TRUE(registry.HasComponent<A>(entity));
            EXPECT_TRUE(registry.HasComponent<B>(entity));
        }
        for (EntityID entity = 420; entity < 1000; entity++)
        {
            EXPECT_FALSE(registry.HasComponent<A>(entity));
            EXPECT_TRUE(registry.HasComponent<B>(entity));
            EXPECT_TRUE(registry.HasComponent<Transform>(entity));
        }
        for (EntityID entity = 1000; entity < 1400; entity++)
        {
            EXPECT_FALSE(registry.HasComponent<B>(entity));
            EXPECT_TRUE(registry.HasComponent<A>(entity));
            EXPECT_TRUE(registry.HasComponent<Transform>(entity));
        }
        for (EntityID entity = 1400; entity < 1800; entity++)
        {
            EXPECT_FALSE(registry.HasComponent<Transform>(entity));
            EXPECT_FALSE(registry.HasComponent<B>(entity));
            EXPECT_TRUE(registry.HasComponent<A>(entity));
        }
    }
    {
        SCOPED_TRACE("Delete Entities");
        for (int i = 696; i < 756; i++)
        {
            registry.DeleteEntity(i);
        }
        for (int i = 1800; i < 2120; i++)
        {
            registry.DeleteEntity(i);
        }
        {
            SCOPED_TRACE("Flush");
            registry.Flush();
        }
        for (int i = 696; i < 756; i++)
        {
            EXPECT_FALSE(registry.IsEntityValid(i));
        }
        for (int i = 1800; i < 2120; i++)
        {
            EXPECT_FALSE(registry.IsEntityValid(i));
        }
    }
    {
        SCOPED_TRACE("Verify Component Values");
        ScopeTimer timer("Verify Component Values");
        for (EntityID entity = 0; entity < 8192; entity++)
        {
            if (entity >= 100 && entity < 420)
            {
                EXPECT_FALSE(registry.HasComponent<Transform>(entity));
                EXPECT_TRUE(registry.HasComponent<A>(entity));
                EXPECT_TRUE(registry.HasComponent<B>(entity));
                EXPECT_EQ(registry.GetComponent<A>(entity).Hello, A{ (int)entity }.Hello);
                EXPECT_EQ(registry.GetComponent<B>(entity).s, B{ "Entity" + std::to_string(entity) }.s);
            }
            else if (entity >= 420 && entity < 1000)
            {
                if (entity >= 696 && entity < 756)
                {
                    EXPECT_FALSE(registry.IsEntityValid(entity));
                    continue;
                }
                EXPECT_FALSE(registry.HasComponent<A>(entity));
                EXPECT_TRUE(registry.HasComponent<B>(entity));
                EXPECT_TRUE(registry.HasComponent<Transform>(entity));
                EXPECT_EQ(registry.GetComponent<B>(entity).s, B{ "Entity" + std::to_string(entity) }.s);
                auto t = Transform{ {(float)-(int)entity, (float)entity, (float)entity * 10}, {0, 0, 0}, {1, 1, 1} };
                EXPECT_EQ(registry.GetComponent<Transform>(entity).Position, t.Position);
            }
            else if (entity >= 1000 && entity < 1400)
            {
                EXPECT_FALSE(registry.HasComponent<B>(entity));
                EXPECT_TRUE(registry.HasComponent<A>(entity));
                EXPECT_TRUE(registry.HasComponent<Transform>(entity));
                EXPECT_EQ(registry.GetComponent<A>(entity).Hello, A{ (int)entity }.Hello);
                auto t = Transform{ {(float)-(int)entity, (float)entity, (float)entity * 10}, {0, 0, 0}, {1, 1, 1} };
                EXPECT_EQ(registry.GetComponent<Transform>(entity).Position, t.Position);
            }
            else if (entity >= 1400 && entity < 1800)
            {
                EXPECT_FALSE(registry.HasComponent<Transform>(entity));
                EXPECT_FALSE(registry.HasComponent<B>(entity));
                EXPECT_TRUE(registry.HasComponent<A>(entity));
                EXPECT_EQ(registry.GetComponent<A>(entity).Hello, A{ (int)entity }.Hello);
            }
            else if (entity >= 1800 && entity < 2120)
            {
                EXPECT_FALSE(registry.IsEntityValid(entity));
            }
            else
            {
                EXPECT_TRUE(registry.HasComponent<Transform>(entity));
                EXPECT_TRUE(registry.HasComponent<A>(entity));
                EXPECT_TRUE(registry.HasComponent<B>(entity));
            }
        }
    }
}

TEST_F(EntityRegistryTest, AddAndVerifyComponents)
{
    EntityRegistry registry;

    EntityID entity0 = registry.CreateEntity();
    Transform transform{ {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    A a{ 42 };

    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity0, a);
    EXPECT_EQ(registry.GetComponent<Transform>(entity0).Position, transform.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity0).Hello, a.Hello);
}

TEST_F(EntityRegistryTest, DeleteComponentsPreservesOtherComponents)
{
    EntityRegistry registry;

    EntityID entity0 = registry.CreateEntity();
    Transform transform{ {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    A a{ 42 };

    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity0, a);

    registry.DeleteComponent<Transform>(entity0);
    registry.Flush();

    EXPECT_FALSE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<A>(entity0));
    EXPECT_EQ(registry.GetComponent<A>(entity0).Hello, a.Hello);
}

TEST_F(EntityRegistryTest, DeleteEntityCleansUpComponents)
{
    EntityRegistry registry;
    EntityID entity0 = registry.CreateEntity();
    EXPECT_TRUE(registry.IsEntityValid(entity0));
    Transform transform{ {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    A a{ 42 };

    registry.TryAddComponent(entity0, transform);
    registry.TryAddComponent(entity0, a);

    registry.DeleteEntity(entity0);
    registry.Flush();

    EXPECT_FALSE(registry.IsEntityValid(entity0));
    EXPECT_FALSE(registry.HasComponent<Transform>(entity0));
    EXPECT_FALSE(registry.HasComponent<A>(entity0));
}

TEST_F(EntityRegistryTest, MultipleEntitiesWithSameSignature)
{
    EntityRegistry registry;
    Transform t1{ {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    Transform t2{ {4.0f, 5.0f, 6.0f}, {0,0,0}, {1,1,1} };
    Transform t3{ {7.0f, 8.0f, 9.0f}, {0,0,0}, {1,1,1} };
    Transform t4{ {10.0f, 11.0f, 12.0f}, {0,0,0}, {1,1,1} };
    Transform t5{ {13.0f, 14.0f, 15.0f}, {0,0,0}, {1,1,1} };
    A a1{ 42 };
    A a2{ 84 };
    A a3{ 126 };
    A a4{ 168 };
    A a5{ 210 };

    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();
    EntityID entity3 = registry.CreateEntity();
    EntityID entity4 = registry.CreateEntity();

    registry.TryAddComponent(entity0, t1);
    registry.TryAddComponent(entity0, a1);
    registry.TryAddComponent(entity1, t2);
    registry.TryAddComponent(entity1, a2);
    registry.TryAddComponent(entity2, t3);
    registry.TryAddComponent(entity2, a3);
    registry.TryAddComponent(entity3, t4);
    registry.TryAddComponent(entity3, a4);
    registry.TryAddComponent(entity4, t5);
    registry.TryAddComponent(entity4, a5);

    EXPECT_EQ(registry.GetComponent<Transform>(entity0).Position, t1.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity0).Hello, a1.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity1).Position, t2.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity1).Hello, a2.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity2).Position, t3.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity2).Hello, a3.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity3).Position, t4.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity3).Hello, a4.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity4).Position, t5.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity4).Hello, a5.Hello);

    registry.DeleteComponent<Transform>(entity0);
    registry.Flush();

    EXPECT_FALSE(registry.HasComponent<Transform>(entity0));
    EXPECT_EQ(registry.GetComponent<A>(entity0).Hello, a1.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity1).Position, t2.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity1).Hello, a2.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity2).Position, t3.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity2).Hello, a3.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity3).Position, t4.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity3).Hello, a4.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity4).Position, t5.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity4).Hello, a5.Hello);
}

TEST_F(EntityRegistryTest, MigrateEntityBetweenArchetypes)
{
    EntityRegistry registry;
    Transform t{ {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    A a{ 42 };
    B b{ "SD" };

    EntityID entity = registry.CreateEntity();

    registry.TryAddComponent(entity, t);
    registry.TryAddComponent(entity, a);

    EXPECT_EQ(registry.GetComponent<Transform>(entity).Position, t.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity).Hello, a.Hello);

    registry.TryAddComponent(entity, b);

    EXPECT_EQ(registry.GetComponent<B>(entity).s, b.s);

    registry.DeleteComponent<A>(entity);
    registry.Flush();

    EXPECT_FALSE(registry.HasComponent<A>(entity));
    EXPECT_EQ(registry.GetComponent<Transform>(entity).Position, t.Position);
    EXPECT_EQ(registry.GetComponent<B>(entity).s, b.s);
}

TEST_F(EntityRegistryTest, VerifyStateAfterComplexOperations)
{
    EntityRegistry registry;
    Transform t1{ {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    Transform t2{ {4.0f, 5.0f, 6.0f}, {0,0,0}, {1,1,1} };
    Transform t3{ {7.0f, 8.0f, 9.0f}, {0,0,0}, {1,1,1} };
    Transform t4{ {10.0f, 11.0f, 12.0f}, {0,0,0}, {1,1,1} };
    Transform t5{ {13.0f, 14.0f, 15.0f}, {0,0,0}, {1,1,1} };
    A a1{ 42 };
    A a2{ 84 };
    A a3{ 126 };
    A a4{ 168 };
    A a5{ 210 };
    B b1{ "S1" };
    B b2{ "S2" };

    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();
    EntityID entity3 = registry.CreateEntity();
    EntityID entity4 = registry.CreateEntity();

    registry.TryAddComponent(entity0, t1);
    registry.TryAddComponent(entity0, a1);
    registry.TryAddComponent(entity1, t2);
    registry.TryAddComponent(entity1, b1);
    registry.TryAddComponent(entity2, t3);
    registry.TryAddComponent(entity2, a2);
    registry.TryAddComponent(entity3, t4);
    registry.TryAddComponent(entity3, b2);
    registry.TryAddComponent(entity4, t5);
    registry.TryAddComponent(entity4, a3);

    registry.DeleteComponent<Transform>(entity0);
    registry.DeleteEntity(entity1);

    registry.Flush();

    EXPECT_FALSE(registry.IsEntityValid(entity1));
    EXPECT_FALSE(registry.HasComponent<Transform>(entity0));
    EXPECT_TRUE(registry.HasComponent<A>(entity0));
    EXPECT_EQ(registry.GetComponent<A>(entity0).Hello, a1.Hello);

    EXPECT_FALSE(registry.HasComponent<Transform>(entity1));
    EXPECT_FALSE(registry.HasComponent<B>(entity1));

    EXPECT_EQ(registry.GetComponent<Transform>(entity2).Position, t3.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity2).Hello, a2.Hello);

    EXPECT_EQ(registry.GetComponent<Transform>(entity3).Position, t4.Position);
    EXPECT_EQ(registry.GetComponent<B>(entity3).s, b2.s);

    EXPECT_EQ(registry.GetComponent<Transform>(entity4).Position, t5.Position);
    EXPECT_EQ(registry.GetComponent<A>(entity4).Hello, a3.Hello);
}

TEST_F(EntityRegistryTest, ComponentViewTest)
{
    EntityRegistry registry;
    Transform t[5];
    t[0] = { {1.0f, 2.0f, 3.0f}, {0,0,0}, {1,1,1} };
    t[1] = { {4.0f, 5.0f, 6.0f}, {0,0,0}, {1,1,1} };
    t[2] = { {7.0f, 8.0f, 9.0f}, {0,0,0}, {1,1,1} };
    t[3] = { {10.0f, 11.0f, 12.0f}, {0,0,0}, {1,1,1} };
    t[4] = { {13.0f, 14.0f, 15.0f}, {0,0,0}, {1,1,1} };
    A a1{ 42 };
    A a2{ 84 };
    A a3{ 126 };
    B b1{ "S1" };
    B b2{ "S2" };

    EntityID entity0 = registry.CreateEntity();
    EntityID entity1 = registry.CreateEntity();
    EntityID entity2 = registry.CreateEntity();
    EntityID entity3 = registry.CreateEntity();
    EntityID entity4 = registry.CreateEntity();

    registry.TryAddComponent(entity0, t[0]);
    registry.TryAddComponent(entity0, a1);
    registry.TryAddComponent(entity1, t[1]);
    registry.TryAddComponent(entity1, b1);
    registry.TryAddComponent(entity2, t[2]);
    registry.TryAddComponent(entity2, a2);
    registry.TryAddComponent(entity3, t[3]);
    registry.TryAddComponent(entity3, b2);
    registry.TryAddComponent(entity4, t[4]);
    registry.TryAddComponent(entity4, a3);

    auto view = registry.GetView<Transform>();
    for (auto index : view)
    {
        auto [transform] = view.Get(index);
        EXPECT_EQ(transform.Position, t[index.Entity].Position);
    }
}

class ComponentViewStressTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EntityRegistry::RegisterComponentTypes<Transform, A, B>();
        registry = EntityRegistry(8192);
        for (int i = 0; i < 8192; i++)
        {
            EntityID entity = registry.CreateEntity();
            Transform transform{ {(float)i, (float)i, (float)i * 10}, {0, 0, 0}, {1, 1, 1} };
            A a{ i };
            B b{ "Entity" + std::to_string(i) };
            registry.TryAddComponent(entity, transform);
            registry.TryAddComponent(entity, a);
            registry.TryAddComponent(entity, b);
        }
        for (EntityID entity = 100; entity < 420; entity++)
        {
            registry.DeleteComponent<Transform>(entity);
        }
        for (EntityID entity = 420; entity < 1000; entity++)
        {
            registry.DeleteComponent<A>(entity);
        }
        for (EntityID entity = 1000; entity < 1400; entity++)
        {
            registry.DeleteComponent<B>(entity);
        }
        for (EntityID entity = 1400; entity < 1800; entity++)
        {
            registry.DeleteComponent<Transform>(entity);
            registry.DeleteComponent<B>(entity);
        }
        for (int i = 696; i < 756; i++)
        {
            registry.DeleteEntity(i);
        }
        for (int i = 1800; i < 2120; i++)
        {
            registry.DeleteEntity(i);
        }
        registry.Flush();
    }

    EntityRegistry registry;
};

TEST_F(ComponentViewStressTest, ComponentViewStressTest)
{
    ScopeTimer timer("ComponentViewStressTest");
    auto view = registry.GetView<Transform, A>();
    Transform transform{};
    A a{};
    for (auto index : view)
    {
        transform = { {(float)index.Entity, (float)index.Entity, (float)index.Entity * 10}, {0, 0, 0}, {1, 1, 1} };
        a = { (int)index.Entity };
        auto [transformInView, aInView] = view.Get(index);
        EXPECT_EQ(transformInView.Position, transform.Position);
        EXPECT_EQ(a.Hello, aInView.Hello);
    }
}

}
