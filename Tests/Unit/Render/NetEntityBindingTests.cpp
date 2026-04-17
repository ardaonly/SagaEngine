/// @file NetEntityBindingTests.cpp
/// @brief Lifecycle tests for the network-id → RenderEntity bridge.
///
/// Layer  : SagaEngine / Render / Bridge
/// Purpose: Bind / update / resolve / release under edge conditions —
///          double release, resolve-after-release, updates on unknown
///          ids, and the Clear() teardown path.

#include "SagaEngine/Render/Bridge/NetEntityBinding.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <gtest/gtest.h>

using namespace SagaEngine::Render;
using SagaEngine::Math::Vec3;
using SagaEngine::Math::Transform;
using SagaEngine::Math::Quat;

namespace
{
World::RenderEntity MakeEntity(const Vec3& pos = Vec3{0, 0, 0})
{
    World::RenderEntity e{};
    e.transform.position = pos;
    e.boundsRadius       = 1.0f;
    e.visible            = true;
    return e;
}
} // namespace

// ─── Create / resolve ─────────────────────────────────────────────────

TEST(NetEntityBinding, BindCreatesRenderEntity)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);

    const auto id = bridge.BindOrCreate(1001, MakeEntity(Vec3{1, 2, 3}));
    EXPECT_NE(id, World::RenderEntityId::kInvalid);
    EXPECT_TRUE(world.IsAlive(id));
    EXPECT_EQ(bridge.Resolve(1001), id);
    EXPECT_EQ(bridge.Size(), 1u);
    EXPECT_EQ(world.Get(id)->networkId, 1001u);
}

TEST(NetEntityBinding, RebindUpdatesExistingSlot)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);

    const auto first  = bridge.BindOrCreate(42, MakeEntity(Vec3{0, 0, 0}));
    const auto second = bridge.BindOrCreate(42, MakeEntity(Vec3{9, 9, 9}));
    EXPECT_EQ(first, second);               // Same slot.
    EXPECT_EQ(bridge.Size(), 1u);
    EXPECT_FLOAT_EQ(world.Get(first)->transform.position.x, 9.0f);
    EXPECT_EQ(world.Get(first)->networkId, 42u);  // Preserved across rebind.
}

TEST(NetEntityBinding, ResolveUnknownReturnsInvalid)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    EXPECT_EQ(bridge.Resolve(777), World::RenderEntityId::kInvalid);
}

// ─── Release semantics ────────────────────────────────────────────────

TEST(NetEntityBinding, ReleaseRemovesMappingAndDestroysEntity)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    const auto id = bridge.BindOrCreate(55, MakeEntity());

    bridge.Release(55);
    EXPECT_FALSE(world.IsAlive(id));
    EXPECT_EQ(bridge.Resolve(55), World::RenderEntityId::kInvalid);
    EXPECT_EQ(bridge.Size(), 0u);
}

TEST(NetEntityBinding, DoubleReleaseIsNoOp)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    bridge.BindOrCreate(77, MakeEntity());

    bridge.Release(77);
    // Second release must not crash, must not decrement into negatives,
    // must not affect an unrelated slot.
    bridge.Release(77);
    EXPECT_EQ(bridge.Size(), 0u);

    // Unrelated bind still works.
    const auto next = bridge.BindOrCreate(78, MakeEntity());
    EXPECT_TRUE(world.IsAlive(next));
}

TEST(NetEntityBinding, ReleaseOfUnknownIdIsSafe)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    bridge.Release(999);  // Never bound.
    EXPECT_EQ(bridge.Size(), 0u);
}

// ─── Hot-path updates ─────────────────────────────────────────────────

TEST(NetEntityBinding, UpdateTransformReachesRenderWorld)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    const auto id = bridge.BindOrCreate(1, MakeEntity());

    Transform t{Vec3{10, 20, 30}, Quat::Identity(), Vec3::One()};
    EXPECT_TRUE(bridge.UpdateTransform(1, t));
    EXPECT_FLOAT_EQ(world.Get(id)->transform.position.x, 10.0f);
}

TEST(NetEntityBinding, UpdatesOnUnknownIdFail)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    EXPECT_FALSE(bridge.UpdateVisible(1234, false));
    EXPECT_FALSE(bridge.UpdateMesh(1234, static_cast<World::MeshId>(1)));
    EXPECT_FALSE(bridge.UpdateMaterial(1234, static_cast<World::MaterialId>(1)));
    EXPECT_FALSE(bridge.UpdateBoundsRadius(1234, 10.0f));
}

TEST(NetEntityBinding, UpdateAfterReleaseReturnsFalse)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    bridge.BindOrCreate(5, MakeEntity());
    bridge.Release(5);
    EXPECT_FALSE(bridge.UpdateVisible(5, true));
}

// ─── Clear / teardown ─────────────────────────────────────────────────

TEST(NetEntityBinding, ClearDestroysAllBridgedEntities)
{
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);
    const auto a = bridge.BindOrCreate(1, MakeEntity());
    const auto b = bridge.BindOrCreate(2, MakeEntity());
    const auto c = bridge.BindOrCreate(3, MakeEntity());

    bridge.Clear();
    EXPECT_EQ(bridge.Size(), 0u);
    EXPECT_FALSE(world.IsAlive(a));
    EXPECT_FALSE(world.IsAlive(b));
    EXPECT_FALSE(world.IsAlive(c));
}

TEST(NetEntityBinding, ClearDoesNotTouchStandaloneClientEntities)
{
    // Purely client-side entities (created directly on the RenderWorld,
    // never bound to a network id) must survive bridge.Clear().
    World::RenderWorld world;
    Bridge::NetEntityBinding bridge(world);

    const auto standalone = world.Create(MakeEntity(Vec3{1, 2, 3}));
    bridge.BindOrCreate(42, MakeEntity());

    bridge.Clear();
    EXPECT_EQ(bridge.Size(), 0u);
    EXPECT_TRUE(world.IsAlive(standalone));
    EXPECT_EQ(world.LiveCount(), 1u);
}
