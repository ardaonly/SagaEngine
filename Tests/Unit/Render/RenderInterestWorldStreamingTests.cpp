#include "SagaEngine/Render/World/RenderInterestWorldStreaming.h"

#include <gtest/gtest.h>

#include <string>

using namespace SagaEngine::Render;

TEST(RenderInterestWorldStreaming, NetworkRelevanceDoesNotImplyRenderVisibility)
{
    const auto record = World::EvaluateRenderInterestRecord(
        static_cast<World::RenderEntityId>(1),
        99,
        true,
        false,
        false,
        false);

    EXPECT_EQ(record.interest, World::RenderInterestState::NetworkRelevant);
    EXPECT_EQ(record.visibility, World::RenderVisibilityState::Hidden);
    ASSERT_FALSE(record.diagnostics.empty());
    EXPECT_EQ(record.diagnostics.front().id, "RenderInterest.NetworkOnly");
}

TEST(RenderInterestWorldStreaming, RenderRelevantEntityRequestsPreloadBeforeRegionLoaded)
{
    World::RenderStreamingPreloadHint hint;
    hint.entity = static_cast<World::RenderEntityId>(2);
    hint.assetId = 42;
    hint.assetKind = Streaming::RenderStreamingAssetKind::Geometry;
    hint.priorityScore = 100;

    const auto record = World::EvaluateRenderInterestRecord(
        static_cast<World::RenderEntityId>(2),
        100,
        true,
        true,
        false,
        false,
        {hint});

    EXPECT_EQ(record.interest, World::RenderInterestState::StreamingRelevant);
    EXPECT_EQ(record.visibility, World::RenderVisibilityState::PendingStreaming);
    ASSERT_EQ(record.preloadHints.size(), 1u);
    EXPECT_EQ(record.diagnostics.front().id, "RenderInterest.PreloadHint");
}

TEST(RenderInterestWorldStreaming, LoadedRegionWithoutResourcesUsesFallback)
{
    const auto record = World::EvaluateRenderInterestRecord(
        static_cast<World::RenderEntityId>(3),
        101,
        true,
        true,
        true,
        false);

    EXPECT_EQ(record.interest, World::RenderInterestState::RenderVisible);
    EXPECT_EQ(record.visibility, World::RenderVisibilityState::Fallback);
    ASSERT_FALSE(record.diagnostics.empty());
    EXPECT_EQ(record.diagnostics.front().id,
              "RenderInterest.UnloadedZoneFallback");
}

TEST(RenderInterestWorldStreaming, ResidentResourcesEnterVisibility)
{
    const auto record = World::EvaluateRenderInterestRecord(
        static_cast<World::RenderEntityId>(4),
        102,
        true,
        true,
        true,
        true);

    EXPECT_EQ(record.interest, World::RenderInterestState::RenderVisible);
    EXPECT_EQ(record.visibility, World::RenderVisibilityState::Visible);
    ASSERT_FALSE(record.diagnostics.empty());
    EXPECT_EQ(record.diagnostics.front().id,
              "RenderInterest.EnteredVisibility");
}

TEST(RenderInterestWorldStreaming, ReportSerializationIsDeterministic)
{
    auto late = World::EvaluateRenderInterestRecord(
        static_cast<World::RenderEntityId>(10),
        110,
        true,
        true,
        true,
        true);

    auto early = World::EvaluateRenderInterestRecord(
        static_cast<World::RenderEntityId>(2),
        102,
        true,
        true,
        false,
        false,
        {
            {
                .entity = static_cast<World::RenderEntityId>(2),
                .assetId = 50,
                .assetKind = Streaming::RenderStreamingAssetKind::Texture,
                .requestedDetail = 2,
                .priorityScore = 20,
            },
            {
                .entity = static_cast<World::RenderEntityId>(2),
                .assetId = 10,
                .assetKind = Streaming::RenderStreamingAssetKind::Geometry,
                .requestedDetail = 1,
                .priorityScore = 40,
            },
        });

    const auto first = World::BuildRenderInterestReport({late, early});
    const auto second = World::BuildRenderInterestReport({late, early});

    EXPECT_EQ(first.Serialize(), second.Serialize());
    const auto text = first.Serialize();
    EXPECT_LT(text.find("record entity=2"), text.find("record entity=10"));
    EXPECT_LT(text.find("asset=10"), text.find("asset=50"));
    EXPECT_NE(text.find("RenderInterest.PreloadHint"), std::string::npos);
}
