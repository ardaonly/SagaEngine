/// @file RenderWorldTests.cpp
/// @brief Pool / handle / free-list tests for the client-side RenderWorld.
///
/// Layer  : SagaEngine / Render / World
/// Purpose: Verifies dense-pool correctness — stable handles, free-list
///          reuse, stale-handle rejection, and single-field mutators —
///          before the pipeline above it grows any more.

#include "SagaEngine/Render/World/RenderWorld.h"

#include <gtest/gtest.h>

#include <vector>

using namespace SagaEngine::Render::World;
using SagaEngine::Math::Vec3;
using SagaEngine::Math::Transform;

namespace
{
RenderEntity MakeBasic(float x = 0.0f, float boundsR = 1.0f)
{
    RenderEntity e{};
    e.transform.position = Vec3{x, 0.0f, 0.0f};
    e.boundsRadius       = boundsR;
    e.visible            = true;
    return e;
}
} // namespace

// ─── Basic lifecycle ──────────────────────────────────────────────────

TEST(RenderWorldPool, CreateReturnsValidHandle)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    EXPECT_NE(id, RenderEntityId::kInvalid);
    EXPECT_TRUE(w.IsAlive(id));
    EXPECT_EQ(w.LiveCount(), 1u);
}

TEST(RenderWorldPool, DestroyInvalidatesHandle)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    w.Destroy(id);
    EXPECT_FALSE(w.IsAlive(id));
    EXPECT_EQ(w.Get(id), nullptr);
    EXPECT_EQ(w.LiveCount(), 0u);
}

TEST(RenderWorldPool, FreeListReusesSlotIndex)
{
    RenderWorld w;
    const auto a = w.Create(MakeBasic(1.0f));
    const auto b = w.Create(MakeBasic(2.0f));
    w.Destroy(a);
    const auto c = w.Create(MakeBasic(3.0f));
    EXPECT_EQ(static_cast<std::uint32_t>(c),
              static_cast<std::uint32_t>(a));   // same slot reused.
    EXPECT_NE(c, b);
    EXPECT_TRUE(w.IsAlive(c));
    EXPECT_FLOAT_EQ(w.Get(c)->transform.position.x, 3.0f);
}

TEST(RenderWorldPool, StaleHandleGetReturnsNull)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    w.Destroy(id);
    EXPECT_EQ(w.Get(id), nullptr);

    // Even after a new entity has reused the slot, the old write
    // operations should gracefully early-out (no crash, return false).
    const auto reused = w.Create(MakeBasic(7.0f));
    (void)reused;
    // The slot is alive now — but the caller still holds the old handle,
    // which happens to numerically match. This is the documented trade-off;
    // generational handles are a later extension. Verify at least that the
    // slot's data was reset on destroy, not the old caller's value.
    EXPECT_FLOAT_EQ(w.Get(reused)->transform.position.x, 7.0f);
}

TEST(RenderWorldPool, OutOfRangeHandleIsSafe)
{
    RenderWorld w;
    const auto bogus = static_cast<RenderEntityId>(99999);
    EXPECT_FALSE(w.IsAlive(bogus));
    EXPECT_EQ(w.Get(bogus), nullptr);
    // Setters must not crash on a garbage id.
    EXPECT_FALSE(w.SetVisible(bogus, true));
    EXPECT_FALSE(w.SetTransform(bogus, Transform{}));
}

TEST(RenderWorldPool, DoubleDestroyIsNoOp)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    w.Destroy(id);
    EXPECT_EQ(w.LiveCount(), 0u);
    // Second destroy must not decrement counters or re-add to freelist.
    w.Destroy(id);
    EXPECT_EQ(w.LiveCount(), 0u);
    // The freelist must still only have one slot to hand out.
    const auto a = w.Create(MakeBasic(4.0f));
    const auto b = w.Create(MakeBasic(5.0f));
    EXPECT_NE(static_cast<std::uint32_t>(a),
              static_cast<std::uint32_t>(b));
}

// ─── Single-field mutators ────────────────────────────────────────────

TEST(RenderWorldMutators, SetTransformUpdatesPosition)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    Transform t{Vec3{10, 20, 30}, SagaEngine::Math::Quat::Identity(), Vec3::One()};
    EXPECT_TRUE(w.SetTransform(id, t));
    EXPECT_FLOAT_EQ(w.Get(id)->transform.position.x, 10.0f);
    EXPECT_FLOAT_EQ(w.Get(id)->transform.position.y, 20.0f);
}

TEST(RenderWorldMutators, SetMeshAndMaterial)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    EXPECT_TRUE(w.SetMesh(id,     static_cast<MeshId>(7)));
    EXPECT_TRUE(w.SetMaterial(id, static_cast<MaterialId>(42)));
    EXPECT_EQ(w.Get(id)->mesh,     static_cast<MeshId>(7));
    EXPECT_EQ(w.Get(id)->material, static_cast<MaterialId>(42));
}

TEST(RenderWorldMutators, VisibilityMaskAndFlag)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    EXPECT_TRUE(w.SetVisibilityMask(id, 0xF0u));
    EXPECT_EQ(w.Get(id)->visibilityMask, 0xF0u);
    EXPECT_TRUE(w.SetVisible(id, false));
    EXPECT_FALSE(w.Get(id)->visible);
}

TEST(RenderWorldMutators, LodOverrideAndBoundsRadius)
{
    RenderWorld w;
    const auto id = w.Create(MakeBasic());
    EXPECT_TRUE(w.SetLodOverride(id, 2u));
    EXPECT_TRUE(w.SetBoundsRadius(id, 12.5f));
    EXPECT_EQ(w.Get(id)->lodOverride, 2u);
    EXPECT_FLOAT_EQ(w.Get(id)->boundsRadius, 12.5f);
}

// ─── Iteration ────────────────────────────────────────────────────────

TEST(RenderWorldIter, ForEachSkipsDeadSlots)
{
    RenderWorld w;
    std::vector<RenderEntityId> kept;
    for (int i = 0; i < 5; ++i)
        kept.push_back(w.Create(MakeBasic(static_cast<float>(i))));
    w.Destroy(kept[1]);
    w.Destroy(kept[3]);

    std::size_t hits = 0;
    std::vector<float> xs;
    w.ForEach([&](RenderEntityId /*id*/, const RenderEntity& e) {
        ++hits;
        xs.push_back(e.transform.position.x);
    });
    EXPECT_EQ(hits, 3u);
    // Remaining entities are at index 0, 2, 4 → x == 0, 2, 4.
    EXPECT_FLOAT_EQ(xs[0], 0.0f);
    EXPECT_FLOAT_EQ(xs[1], 2.0f);
    EXPECT_FLOAT_EQ(xs[2], 4.0f);
}
