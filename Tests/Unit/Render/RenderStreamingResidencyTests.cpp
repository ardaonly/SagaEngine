#include "SagaEngine/Render/Streaming/RenderStreamingResidency.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace SagaEngine::Render::Streaming;

namespace {

RenderTextureMipRange Mips(std::uint8_t first, std::uint8_t last)
{
    return {.firstMip = first, .lastMip = last, .valid = true};
}

RenderTexturePoolBudget TextureBudget()
{
    return {
        .softLimitBytes = 64u * 1024u * 1024u,
        .hardLimitBytes = 96u * 1024u * 1024u,
    };
}

RenderGeometryPoolBudget GeometryBudget()
{
    return {
        .softLimitBytes = 64u * 1024u * 1024u,
        .hardLimitBytes = 96u * 1024u * 1024u,
    };
}

} // namespace

TEST(RenderStreamingResidencyTests, TextureTransitionsFromRequestToResident)
{
    RenderTextureResidencyInput input;
    input.assetId = 1001;
    input.cameraDistance = 12.0f;
    input.materialImportance = 1.0f;
    input.criticalVisible = true;
    input.authoredMipCount = 4;
    input.availableMipRange = Mips(0, 3);
    input.estimatedRequestBytes = 8u * 1024u * 1024u;

    auto decision = EvaluateRenderTextureResidency(input, TextureBudget());
    EXPECT_EQ(decision.state, RenderTextureResidencyState::Requested);
    EXPECT_EQ(decision.targetMip, 0u);
    EXPECT_EQ(decision.requestedMipRange.firstMip, 0u);
    EXPECT_EQ(decision.priority.priority, SagaEngine::Resources::StreamingPriority::Critical);

    input.inFlightMipRange = Mips(0, 3);
    decision = EvaluateRenderTextureResidency(input, TextureBudget());
    EXPECT_EQ(decision.state, RenderTextureResidencyState::Requested);

    input.inFlightMipRange = {};
    input.residentMipRange = Mips(0, 3);
    input.estimatedResidentBytes = 8u * 1024u * 1024u;
    decision = EvaluateRenderTextureResidency(input, TextureBudget());
    EXPECT_EQ(decision.state, RenderTextureResidencyState::Resident);
    EXPECT_TRUE(decision.diagnostics.empty());
}

TEST(RenderStreamingResidencyTests, TextureMissingMipFallsBackToCoarserResidentMip)
{
    RenderTextureResidencyInput input;
    input.assetId = 1002;
    input.cameraDistance = 5.0f;
    input.materialImportance = 1.0f;
    input.criticalVisible = true;
    input.authoredMipCount = 5;
    input.availableMipRange = Mips(0, 4);
    input.residentMipRange = Mips(2, 4);

    const auto decision = EvaluateRenderTextureResidency(input, TextureBudget());
    ASSERT_EQ(decision.state, RenderTextureResidencyState::Fallback);
    EXPECT_EQ(decision.fallbackMip, 2u);
    ASSERT_FALSE(decision.diagnostics.empty());
    EXPECT_EQ(decision.diagnostics.front().id,
              "RenderTextureResidency.MissingMipFallback");
}

TEST(RenderStreamingResidencyTests, TextureBudgetPressureCoarsensMipAndReportsOverflow)
{
    RenderTextureResidencyInput input;
    input.assetId = 1003;
    input.cameraDistance = 20.0f;
    input.materialImportance = 1.0f;
    input.authoredMipCount = 6;
    input.availableMipRange = Mips(0, 5);
    input.estimatedRequestBytes = 80u * 1024u * 1024u;

    auto budget = TextureBudget();
    budget.currentResidentBytes = 100u * 1024u * 1024u;
    auto decision = EvaluateRenderTextureResidency(input, budget);
    EXPECT_GE(decision.targetMip, 2u);

    decision.residentBytes = 70u * 1024u * 1024u;
    decision.requestedBytes = 40u * 1024u * 1024u;
    const auto report = BuildRenderTextureMemoryReport({decision}, TextureBudget());
    EXPECT_EQ(report.pressure, RenderBudgetPressure::HardLimitExceeded);
    EXPECT_FALSE(report.diagnostics.empty());
    EXPECT_EQ(report.diagnostics.back().id,
              "RenderTextureResidency.PoolBudgetExceeded");
}

TEST(RenderStreamingResidencyTests, MaterialResidencyHintChangesPriorityWithoutBackendClaim)
{
    RenderTextureResidencyInput normal;
    normal.assetId = 1004;
    normal.cameraDistance = 90.0f;
    normal.materialImportance = 0.4f;
    normal.authoredMipCount = 4;
    normal.availableMipRange = Mips(0, 3);

    auto important = normal;
    important.assetId = 1005;
    important.residencyHint =
        SagaEngine::Render::MaterialTextureResidencyHint::AlwaysResident;

    const auto normalDecision = EvaluateRenderTextureResidency(normal, TextureBudget());
    const auto importantDecision = EvaluateRenderTextureResidency(
        important,
        TextureBudget());

    EXPECT_LT(importantDecision.targetMip, normalDecision.targetMip);
    EXPECT_GT(importantDecision.priority.score, normalDecision.priority.score);
}

TEST(RenderStreamingResidencyTests, GeometryTransitionsAndLodFallback)
{
    RenderGeometryStreamingInput input;
    input.assetId = 2001;
    input.category = RenderGeometryCategory::StaticMesh;
    input.cameraDistance = 260.0f;
    input.materialImportance = 0.8f;
    input.lodCount = 4;
    input.availableLodMask = 0b1111;
    input.residentLodMask = 0b1000;
    input.lodBytes = {8u * 1024u * 1024u, 4u * 1024u * 1024u,
                      2u * 1024u * 1024u, 1u * 1024u * 1024u};

    auto decision = EvaluateRenderGeometryResidency(input, GeometryBudget());
    EXPECT_EQ(decision.targetLod, 2u);
    ASSERT_EQ(decision.state, RenderMeshResidencyState::Fallback);
    EXPECT_EQ(decision.fallbackLod, 3u);
    EXPECT_EQ(decision.diagnostics.front().id,
              "RenderGeometryResidency.LodFallback");

    input.residentLodMask = 0b0100;
    decision = EvaluateRenderGeometryResidency(input, GeometryBudget());
    EXPECT_EQ(decision.state, RenderMeshResidencyState::Resident);
}

TEST(RenderStreamingResidencyTests, SkinnedAndTerrainPoliciesUseSameContract)
{
    RenderGeometryStreamingInput skinned;
    skinned.assetId = 2002;
    skinned.category = RenderGeometryCategory::SkinnedMesh;
    skinned.cameraDistance = 260.0f;
    skinned.materialImportance = 0.7f;
    skinned.lodCount = 4;
    skinned.availableLodMask = 0b1111;
    skinned.lodBytes = {16u, 8u, 4u, 2u};

    RenderGeometryStreamingInput terrain = skinned;
    terrain.assetId = 2003;
    terrain.category = RenderGeometryCategory::TerrainChunk;

    const auto skinnedDecision = EvaluateRenderGeometryResidency(
        skinned,
        GeometryBudget());
    const auto terrainDecision = EvaluateRenderGeometryResidency(
        terrain,
        GeometryBudget());

    EXPECT_LT(skinnedDecision.targetLod, terrainDecision.targetLod);
    EXPECT_EQ(skinnedDecision.category, RenderGeometryCategory::SkinnedMesh);
    EXPECT_EQ(terrainDecision.category, RenderGeometryCategory::TerrainChunk);
}

TEST(RenderStreamingResidencyTests, MissingMeshUsesProxyDiagnostic)
{
    RenderGeometryStreamingInput input;
    input.assetId = 2004;
    input.meshAvailable = false;
    input.lodCount = 2;
    input.availableLodMask = 0;

    const auto decision = EvaluateRenderGeometryResidency(input, GeometryBudget());
    EXPECT_EQ(decision.state, RenderMeshResidencyState::Fallback);
    EXPECT_TRUE(decision.usesProxy);
    ASSERT_FALSE(decision.diagnostics.empty());
    EXPECT_EQ(decision.diagnostics.front().id,
              "RenderGeometryResidency.MissingMeshFallback");
}

TEST(RenderStreamingResidencyTests, GeometryBudgetProducesDeterministicEvictionOrder)
{
    RenderGeometryStreamingInput far;
    far.assetId = 3002;
    far.cameraDistance = 800.0f;
    far.materialImportance = 0.2f;
    far.lodCount = 4;
    far.availableLodMask = 0b1111;
    far.residentLodMask = 0b1000;
    far.lodBytes = {100u, 80u, 40u, 20u};

    RenderGeometryStreamingInput near = far;
    near.assetId = 3001;
    near.cameraDistance = 8.0f;
    near.criticalVisible = true;
    near.residentLodMask = 0b0001;

    auto farDecision = EvaluateRenderGeometryResidency(far, GeometryBudget());
    auto nearDecision = EvaluateRenderGeometryResidency(near, GeometryBudget());
    const auto report = BuildRenderGeometryMemoryReport(
        {nearDecision, farDecision},
        {.softLimitBytes = 16u, .hardLimitBytes = 32u});

    ASSERT_GE(report.evictionCandidates.size(), 2u);
    EXPECT_EQ(report.evictionCandidates.front().assetId, 3002u);
    EXPECT_EQ(report.pressure, RenderBudgetPressure::HardLimitExceeded);
}

TEST(RenderStreamingResidencyTests, PriorityOrderingUsesAssetIdTieBreaker)
{
    std::vector<RenderStreamingPriorityInput> inputs = {
        {.assetId = 42,
         .kind = RenderStreamingAssetKind::Texture,
         .criticalVisible = false,
         .cameraDistance = 128.0f,
         .materialImportance = 0.5f,
         .urgency = 2,
         .estimatedBytes = 1024u},
        {.assetId = 7,
         .kind = RenderStreamingAssetKind::Texture,
         .criticalVisible = false,
         .cameraDistance = 128.0f,
         .materialImportance = 0.5f,
         .urgency = 2,
         .estimatedBytes = 1024u},
    };

    const auto sorted = SortRenderStreamingPriorities(inputs);
    ASSERT_EQ(sorted.size(), 2u);
    EXPECT_EQ(sorted[0].assetId, 7u);
    EXPECT_EQ(sorted[1].assetId, 42u);
}

TEST(RenderStreamingResidencyTests, UnifiedReportSerializationIsByteIdentical)
{
    RenderTextureResidencyInput texture;
    texture.assetId = 4001;
    texture.cameraDistance = 5.0f;
    texture.criticalVisible = true;
    texture.materialImportance = 1.0f;
    texture.authoredMipCount = 4;
    texture.availableMipRange = Mips(0, 3);
    texture.residentMipRange = Mips(2, 3);

    RenderGeometryStreamingInput geometry;
    geometry.assetId = 5001;
    geometry.meshAvailable = false;
    geometry.lodCount = 2;
    geometry.availableLodMask = 0b01;

    const auto textureDecision = EvaluateRenderTextureResidency(
        texture,
        TextureBudget());
    const auto geometryDecision = EvaluateRenderGeometryResidency(
        geometry,
        GeometryBudget());

    const auto first = BuildRenderResidencyReport(
        {textureDecision},
        TextureBudget(),
        {geometryDecision},
        GeometryBudget());
    const auto second = BuildRenderResidencyReport(
        {textureDecision},
        TextureBudget(),
        {geometryDecision},
        GeometryBudget());

    EXPECT_EQ(first.Serialize(), second.Serialize());
    EXPECT_NE(first.Serialize().find("RenderTextureResidency.MissingMipFallback"),
              std::string::npos);
    EXPECT_NE(first.Serialize().find("RenderGeometryResidency.MissingMeshFallback"),
              std::string::npos);
}
