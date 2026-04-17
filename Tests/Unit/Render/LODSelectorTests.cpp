/// @file LODSelectorTests.cpp
/// @brief Boundary and quality-preset behaviour of the LOD selector.
///
/// Layer  : SagaEngine / Render / LOD
/// Purpose: Locks the "distance >= scaled threshold" rule at boundary
///          conditions (epsilon on each side) and verifies each quality
///          preset's multiplier applies correctly to both the linear and
///          squared-distance selection paths.

#include "SagaEngine/Render/LOD/LODSelector.h"

#include <gtest/gtest.h>

using namespace SagaEngine::Render::LOD;

namespace
{
LodThresholds MakeThresholds()
{
    LodThresholds t{};
    t.byLevel = {0.0f, 20.0f, 80.0f, 200.0f};
    t.count   = 4;
    return t;
}
} // namespace

// ─── Basic selection ──────────────────────────────────────────────────

TEST(LodSelector, Lod0ForNearDistances)
{
    const auto t = MakeThresholds();
    EXPECT_EQ(SelectLod(t,  0.0f, QualityPreset::High), 0u);
    EXPECT_EQ(SelectLod(t, 10.0f, QualityPreset::High), 0u);
    EXPECT_EQ(SelectLod(t, 19.9f, QualityPreset::High), 0u);
}

TEST(LodSelector, Lod1AtFirstThreshold)
{
    const auto t = MakeThresholds();
    EXPECT_EQ(SelectLod(t, 20.0f, QualityPreset::High), 1u);
    EXPECT_EQ(SelectLod(t, 50.0f, QualityPreset::High), 1u);
}

TEST(LodSelector, TopLodForExcessiveDistance)
{
    const auto t = MakeThresholds();
    EXPECT_EQ(SelectLod(t, 10'000.0f, QualityPreset::High), 3u);
}

TEST(LodSelector, EmptyThresholdsReturnZero)
{
    LodThresholds empty{};
    empty.count = 0;
    EXPECT_EQ(SelectLod(empty, 5000.0f, QualityPreset::High), 0u);
}

// ─── Boundary behaviour ───────────────────────────────────────────────

TEST(LodSelectorBoundary, ExactThresholdPicksNextLod)
{
    const auto t = MakeThresholds();
    // distance >= threshold → step up.
    EXPECT_EQ(SelectLod(t, 20.0f, QualityPreset::High), 1u);
    EXPECT_EQ(SelectLod(t, 80.0f, QualityPreset::High), 2u);
    EXPECT_EQ(SelectLod(t, 200.0f, QualityPreset::High), 3u);
}

TEST(LodSelectorBoundary, EpsilonBelowStaysOnPrevLod)
{
    const auto t = MakeThresholds();
    constexpr float eps = 0.0001f;
    EXPECT_EQ(SelectLod(t, 20.0f  - eps, QualityPreset::High), 0u);
    EXPECT_EQ(SelectLod(t, 80.0f  - eps, QualityPreset::High), 1u);
    EXPECT_EQ(SelectLod(t, 200.0f - eps, QualityPreset::High), 2u);
}

TEST(LodSelectorBoundary, EpsilonAboveCrossesIntoNextLod)
{
    const auto t = MakeThresholds();
    constexpr float eps = 0.0001f;
    EXPECT_EQ(SelectLod(t, 20.0f  + eps, QualityPreset::High), 1u);
    EXPECT_EQ(SelectLod(t, 80.0f  + eps, QualityPreset::High), 2u);
    EXPECT_EQ(SelectLod(t, 200.0f + eps, QualityPreset::High), 3u);
}

// ─── Quality preset multipliers ───────────────────────────────────────

TEST(LodSelectorQuality, LowPullsThresholdsIn)
{
    // scale = 0.5 → effective thresholds: 0, 10, 40, 100.
    const auto t = MakeThresholds();
    EXPECT_EQ(SelectLod(t,   9.9f, QualityPreset::Low), 0u);
    EXPECT_EQ(SelectLod(t,  10.0f, QualityPreset::Low), 1u);
    EXPECT_EQ(SelectLod(t,  40.0f, QualityPreset::Low), 2u);
    EXPECT_EQ(SelectLod(t, 100.0f, QualityPreset::Low), 3u);
}

TEST(LodSelectorQuality, UltraPushesThresholdsOut)
{
    // scale = 1.5 → effective thresholds: 0, 30, 120, 300.
    const auto t = MakeThresholds();
    EXPECT_EQ(SelectLod(t,  29.9f, QualityPreset::Ultra), 0u);
    EXPECT_EQ(SelectLod(t,  30.0f, QualityPreset::Ultra), 1u);
    EXPECT_EQ(SelectLod(t, 120.0f, QualityPreset::Ultra), 2u);
    EXPECT_EQ(SelectLod(t, 300.0f, QualityPreset::Ultra), 3u);
}

TEST(LodSelectorQuality, MediumPreset)
{
    // scale = 0.75 → effective thresholds: 0, 15, 60, 150.
    const auto t = MakeThresholds();
    EXPECT_EQ(SelectLod(t, 15.0f,  QualityPreset::Medium), 1u);
    EXPECT_EQ(SelectLod(t, 60.0f,  QualityPreset::Medium), 2u);
    EXPECT_EQ(SelectLod(t, 150.0f, QualityPreset::Medium), 3u);
}

TEST(LodSelectorQuality, CameraLodBiasStacksOnPreset)
{
    const auto t = MakeThresholds();
    // Preset High (1.0) × cameraLodBias 2.0 → thresholds 0, 40, 160, 400.
    EXPECT_EQ(SelectLod(t, 39.9f, QualityPreset::High, 2.0f), 0u);
    EXPECT_EQ(SelectLod(t, 40.0f, QualityPreset::High, 2.0f), 1u);
    EXPECT_EQ(SelectLod(t, 200.0f, QualityPreset::High, 2.0f), 2u);
}

// ─── Squared-distance variant must agree ──────────────────────────────

TEST(LodSelectorSquared, AgreesWithLinearForRepresentativeDistances)
{
    const auto t = MakeThresholds();
    const float distances[] = { 0.0f, 10.0f, 20.0f, 25.0f, 79.99f, 80.0f,
                                  150.0f, 199.99f, 200.0f, 500.0f };
    for (float d : distances)
    {
        const auto l1 = SelectLod  (t, d,        QualityPreset::High);
        const auto l2 = SelectLodSq(t, d * d,    QualityPreset::High);
        EXPECT_EQ(l1, l2);   // mismatch at `d` indicates squared path drift.
    }
}

TEST(LodSelectorSquared, HonoursQualityPresetSquared)
{
    const auto t = MakeThresholds();
    // Low scale = 0.5 → effective first-threshold squared = 10^2 = 100.
    EXPECT_EQ(SelectLodSq(t,  99.0f, QualityPreset::Low), 0u);
    EXPECT_EQ(SelectLodSq(t, 100.0f, QualityPreset::Low), 1u);
}

TEST(LodSelectorQuality, QualityScaleTable)
{
    EXPECT_FLOAT_EQ(QualityScale(QualityPreset::Low),    0.5f);
    EXPECT_FLOAT_EQ(QualityScale(QualityPreset::Medium), 0.75f);
    EXPECT_FLOAT_EQ(QualityScale(QualityPreset::High),   1.0f);
    EXPECT_FLOAT_EQ(QualityScale(QualityPreset::Ultra),  1.5f);
}
