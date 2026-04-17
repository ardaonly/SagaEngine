/// @file ComponentDirtyMaskTests.cpp
/// @brief Unit tests for ComponentMask bit operations and dirty tracking.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: Tests the ComponentMask bit manipulation logic used in the
///          replication pipeline to track which components have changed
///          since the last snapshot sent to a client.  These tests verify:
///            - Bit setting, clearing, and testing
///            - Mask combination (OR, AND, XOR)
///            - Dirty state tracking per entity
///            - Serialization/deserialization of dirty masks
///            - Edge cases (64 components, overflow, underflow)

#include "SagaServer/Networking/Replication/WorldSnapshotWire.h"
#include "SagaEngine/Core/Log/Log.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

using namespace SagaServer;

// ─── ComponentMask Bit Operations ───────────────────────────────────────────

class ComponentMaskTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComponentMaskTest, DefaultMaskIsEmpty)
{
    ComponentMask mask = 0;
    EXPECT_EQ(mask, 0u);
}

TEST_F(ComponentMaskTest, SetSingleBit)
{
    ComponentMask mask = 0;

    mask |= COMPONENT_TRANSFORM;
    EXPECT_TRUE((mask & COMPONENT_TRANSFORM) != 0);
    EXPECT_FALSE((mask & COMPONENT_PHYSICS) != 0);
}

TEST_F(ComponentMaskTest, SetMultipleBits)
{
    ComponentMask mask = 0;

    mask |= COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_RENDER;
    EXPECT_TRUE((mask & COMPONENT_TRANSFORM) != 0);
    EXPECT_TRUE((mask & COMPONENT_PHYSICS) != 0);
    EXPECT_TRUE((mask & COMPONENT_RENDER) != 0);
    EXPECT_FALSE((mask & COMPONENT_ANIMATION) != 0);
}

TEST_F(ComponentMaskTest, ClearSingleBit)
{
    ComponentMask mask = COMPONENT_TRANSFORM | COMPONENT_PHYSICS;

    mask &= ~COMPONENT_TRANSFORM;
    EXPECT_FALSE((mask & COMPONENT_TRANSFORM) != 0);
    EXPECT_TRUE((mask & COMPONENT_PHYSICS) != 0);
}

TEST_F(ComponentMaskTest, ClearAllBits)
{
    ComponentMask mask = COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_RENDER;

    mask = 0;
    EXPECT_EQ(mask, 0u);
}

TEST_F(ComponentMaskTest, ToggleBit)
{
    ComponentMask mask = COMPONENT_TRANSFORM;

    mask ^= COMPONENT_TRANSFORM;
    EXPECT_FALSE((mask & COMPONENT_TRANSFORM) != 0);

    mask ^= COMPONENT_TRANSFORM;
    EXPECT_TRUE((mask & COMPONENT_TRANSFORM) != 0);
}

TEST_F(ComponentMaskTest, CountSetBits)
{
    ComponentMask mask = COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_ANIMATION;

    uint32_t count = 0;
    for (int i = 0; i < 64; ++i)
    {
        if ((mask & (1ULL << i)) != 0)
            count++;
    }

    EXPECT_EQ(count, 3u);
}

TEST_F(ComponentMaskTest, All64Bits)
{
    ComponentMask mask = ~0ULL;  // All bits set.

    uint32_t count = 0;
    for (int i = 0; i < 64; ++i)
    {
        if ((mask & (1ULL << i)) != 0)
            count++;
    }

    EXPECT_EQ(count, 64u);
}

// ─── EntityDirtyState ───────────────────────────────────────────────────────

TEST_F(ComponentMaskTest, EntityDirtyState_Default)
{
    EntityDirtyState state;

    EXPECT_EQ(state.dirtyComponents, 0u);
    EXPECT_EQ(state.activeComponents, 0u);
    EXPECT_EQ(state.lastUpdateTick, 0u);
    EXPECT_FALSE(state.deleted);
}

TEST_F(ComponentMaskTest, EntityDirtyState_MarkDirty)
{
    EntityDirtyState state;

    state.dirtyComponents |= COMPONENT_TRANSFORM;
    state.activeComponents |= COMPONENT_TRANSFORM;
    state.lastUpdateTick = 42;

    EXPECT_TRUE((state.dirtyComponents & COMPONENT_TRANSFORM) != 0);
    EXPECT_TRUE((state.activeComponents & COMPONENT_TRANSFORM) != 0);
    EXPECT_EQ(state.lastUpdateTick, 42u);
}

TEST_F(ComponentMaskTest, EntityDirtyState_ClearDirty)
{
    EntityDirtyState state;

    state.dirtyComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS;
    state.dirtyComponents = 0;  // Clear all.

    EXPECT_EQ(state.dirtyComponents, 0u);
    // activeComponents should remain.
}

TEST_F(ComponentMaskTest, EntityDirtyState_MarkDeleted)
{
    EntityDirtyState state;

    state.deleted = true;
    state.dirtyComponents = COMPONENT_TRANSFORM;  // Deleted entities still have dirty state.

    EXPECT_TRUE(state.deleted);
    EXPECT_TRUE((state.dirtyComponents & COMPONENT_TRANSFORM) != 0);
}

// ─── Dirty Mask Combination ────────────────────────────────────────────────

TEST_F(ComponentMaskTest, CombineDirtyMasks)
{
    EntityDirtyState stateA, stateB;

    stateA.dirtyComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS;
    stateB.dirtyComponents = COMPONENT_RENDER | COMPONENT_ANIMATION;

    // Simulate merging two dirty states (e.g., from two ticks).
    ComponentMask combined = stateA.dirtyComponents | stateB.dirtyComponents;

    EXPECT_TRUE((combined & COMPONENT_TRANSFORM) != 0);
    EXPECT_TRUE((combined & COMPONENT_PHYSICS) != 0);
    EXPECT_TRUE((combined & COMPONENT_RENDER) != 0);
    EXPECT_TRUE((combined & COMPONENT_ANIMATION) != 0);

    uint32_t count = 0;
    for (int i = 0; i < 64; ++i)
    {
        if ((combined & (1ULL << i)) != 0)
            count++;
    }
    EXPECT_EQ(count, 4u);
}

TEST_F(ComponentMaskTest, IntersectDirtyMasks)
{
    EntityDirtyState stateA, stateB;

    stateA.dirtyComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_RENDER;
    stateB.dirtyComponents = COMPONENT_PHYSICS | COMPONENT_RENDER | COMPONENT_ANIMATION;

    // Intersection: components dirty in both states.
    ComponentMask intersection = stateA.dirtyComponents & stateB.dirtyComponents;

    EXPECT_FALSE((intersection & COMPONENT_TRANSFORM) != 0);
    EXPECT_TRUE((intersection & COMPONENT_PHYSICS) != 0);
    EXPECT_TRUE((intersection & COMPONENT_RENDER) != 0);
    EXPECT_FALSE((intersection & COMPONENT_ANIMATION) != 0);
}

TEST_F(ComponentMaskTest, DifferenceDirtyMasks)
{
    EntityDirtyState stateA, stateB;

    stateA.dirtyComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_RENDER;
    stateB.dirtyComponents = COMPONENT_PHYSICS;

    // Difference: components dirty in A but not in B.
    ComponentMask difference = stateA.dirtyComponents & ~stateB.dirtyComponents;

    EXPECT_TRUE((difference & COMPONENT_TRANSFORM) != 0);
    EXPECT_FALSE((difference & COMPONENT_PHYSICS) != 0);
    EXPECT_FALSE((difference & COMPONENT_RENDER) != 0);
}

// ─── Edge Cases ─────────────────────────────────────────────────────────────

TEST_F(ComponentMaskTest, HighestBit)
{
    ComponentMask mask = 1ULL << 63;  // Highest bit (bit 63).

    EXPECT_TRUE((mask & (1ULL << 63)) != 0);
    EXPECT_FALSE((mask & (1ULL << 62)) != 0);
    EXPECT_FALSE((mask & (1ULL << 0)) != 0);
}

TEST_F(ComponentMaskTest, CustomComponentBits)
{
    // Test custom component bits (6-9 are reserved for custom).
    ComponentMask customMask = COMPONENT_CUSTOM_0 | COMPONENT_CUSTOM_3;

    EXPECT_TRUE((customMask & COMPONENT_CUSTOM_0) != 0);
    EXPECT_TRUE((customMask & COMPONENT_CUSTOM_3) != 0);
    EXPECT_FALSE((customMask & COMPONENT_CUSTOM_1) != 0);
    EXPECT_FALSE((customMask & COMPONENT_TRANSFORM) != 0);
}

TEST_F(ComponentMaskTest, ZeroMaskOperations)
{
    ComponentMask mask = 0;

    // OR with zero should not change the mask.
    EXPECT_EQ(mask | 0, 0u);

    // AND with zero should clear all bits.
    EXPECT_EQ(mask & ~0ULL, 0u);

    // XOR with zero should not change the mask.
    EXPECT_EQ(mask ^ 0, 0u);
}

// ─── Dirty State Serialization ──────────────────────────────────────────────

TEST_F(ComponentMaskTest, DirtyStateRoundTrip)
{
    EntityDirtyState original;
    original.dirtyComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_RENDER;
    original.activeComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS | COMPONENT_RENDER | COMPONENT_ANIMATION;
    original.lastUpdateTick = 12345;
    original.deleted = false;

    // Serialize to bytes.
    std::vector<uint8_t> buffer(sizeof(EntityDirtyState));
    std::memcpy(buffer.data(), &original, sizeof(EntityDirtyState));

    // Deserialize.
    EntityDirtyState restored;
    std::memcpy(&restored, buffer.data(), sizeof(EntityDirtyState));

    EXPECT_EQ(restored.dirtyComponents, original.dirtyComponents);
    EXPECT_EQ(restored.activeComponents, original.activeComponents);
    EXPECT_EQ(restored.lastUpdateTick, original.lastUpdateTick);
    EXPECT_EQ(restored.deleted, original.deleted);
}

// ─── Performance: Many Entities ─────────────────────────────────────────────

TEST_F(ComponentMaskTest, ManyEntitiesDirtyTracking)
{
    constexpr uint32_t entityCount = 10000;
    std::vector<EntityDirtyState> states(entityCount);

    // Mark every 3rd entity as dirty with different component masks.
    for (uint32_t i = 0; i < entityCount; i += 3)
    {
        auto& state = states[i];
        state.dirtyComponents = (i % 2 == 0) ? COMPONENT_TRANSFORM : COMPONENT_PHYSICS;
        state.activeComponents = COMPONENT_TRANSFORM | COMPONENT_PHYSICS;
        state.lastUpdateTick = i;
    }

    // Count dirty entities.
    uint32_t dirtyCount = 0;
    for (const auto& state : states)
    {
        if (state.dirtyComponents != 0)
            dirtyCount++;
    }

    // Should be approximately entityCount / 3.
    EXPECT_GE(dirtyCount, entityCount / 3 - 1);
    EXPECT_LE(dirtyCount, entityCount / 3 + 1);
}
