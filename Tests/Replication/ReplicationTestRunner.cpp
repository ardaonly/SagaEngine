/// @file ReplicationTestRunner.cpp
/// @brief GTest wrapper for all replication test functions.
///
/// This file bridges the standalone test functions with the GTest framework
/// by wrapping each test in a TEST() macro for proper discovery and reporting.

#include "DeterminismGuardTest.h"
#include "AdversarialModelTest.h"
#include "ChaosTest.h"
#include "ReplicationFuzzTest.h"
#include "PerformanceBudgetTest.h"
#include "ReplicationRoundTripTest.h"
#include "SchemaMigrationTest.h"
#include "SoakTest.h"

#include <gtest/gtest.h>

namespace SagaEngine::Client::Replication {

// ─── Determinism Guard Tests ────────────────────────────────────────────────

TEST(DeterminismGuardTest, GranularHashConsistency)
{
    auto result = TestGranularHashConsistency(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(DeterminismGuardTest, ApplyOrderIndependence)
{
    auto result = TestApplyOrderIndependence(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(DeterminismGuardTest, FloatCanonicalisation)
{
    auto result = TestFloatCanonicalisation(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(DeterminismGuardTest, EntityIterationDeterminism)
{
    auto result = TestEntityIterationDeterminism(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

// ─── Adversarial Model Tests ────────────────────────────────────────────────

TEST(AdversarialModelTest, CpuStarvation)
{
    auto result = TestCpuStarvation(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(AdversarialModelTest, GapStorm)
{
    auto result = TestGapStorm(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(AdversarialModelTest, ReplayAttack)
{
    auto result = TestReplayAttack(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(AdversarialModelTest, SequenceWrapAround)
{
    auto result = TestSequenceWrapAround(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(AdversarialModelTest, DecodeBomb)
{
    auto result = TestDecodeBomb(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

// ─── Chaos Tests ────────────────────────────────────────────────────────────

TEST(ChaosTest, BasicChaos)
{
    ChaosTestConfig config;
    config.tickCount = 1000;
    config.seed = 42;
    auto result = RunChaosTest(config);
    EXPECT_TRUE(result.success) << "Failed: " << result.errorMessage;
}

TEST(ChaosTest, AllChaosModes)
{
    auto results = RunAllChaosModes(1000, 42);
    for (const auto& result : results) {
        EXPECT_TRUE(result.success) << "Failed: " << result.errorMessage;
    }
}

// ─── Replication Fuzz Tests ─────────────────────────────────────────────────

TEST(ReplicationFuzzTest, FuzzDeltaWireDecoder)
{
    auto result = FuzzDeltaWireDecoder(10000, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.errorMessage;
}

TEST(ReplicationFuzzTest, FuzzSnapshotWireDecoder)
{
    auto result = FuzzSnapshotWireDecoder(10000, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.errorMessage;
}

TEST(ReplicationFuzzTest, ChaosPacketOrdering)
{
    auto result = ChaosPacketOrdering(64, 10, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.errorMessage;
}

// ─── Performance Budget Tests ───────────────────────────────────────────────

TEST(PerformanceBudgetTest, MaxApplyTimePerTick)
{
    auto result = TestMaxApplyTimePerTick();
    EXPECT_TRUE(result.withinBudget) << "Failed: " << result.failureDetail;
}

TEST(PerformanceBudgetTest, WorstCaseEntityBurst)
{
    auto result = TestWorstCaseEntityBurst();
    EXPECT_TRUE(result.withinBudget) << "Failed: " << result.failureDetail;
}

TEST(PerformanceBudgetTest, ComponentDeserializeBudget)
{
    auto result = TestComponentDeserializeBudget();
    EXPECT_TRUE(result.withinBudget) << "Failed: " << result.failureDetail;
}

TEST(PerformanceBudgetTest, ColdCacheCost)
{
    auto result = TestColdCacheCost();
    EXPECT_TRUE(result.withinBudget) << "Failed: " << result.failureDetail;
}

// ─── Replication Round Trip Tests ───────────────────────────────────────────

TEST(ReplicationRoundTripTest, DeltaRoundTrip)
{
    auto result = TestDeltaRoundTrip(100, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(ReplicationRoundTripTest, SnapshotRoundTrip)
{
    auto result = TestSnapshotRoundTrip(100, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(ReplicationRoundTripTest, CrcIntegrity)
{
    auto result = TestCrcIntegrity(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(ReplicationRoundTripTest, SchemaForwardCompat)
{
    auto result = TestSchemaForwardCompat(42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

// ─── Schema Migration Tests ─────────────────────────────────────────────────

TEST(SchemaMigrationTest, MajorMismatch)
{
    auto result = TestMajorMismatch();
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SchemaMigrationTest, MinorForwardCompat)
{
    auto result = TestMinorForwardCompat();
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SchemaMigrationTest, MinorBackwardCompat)
{
    auto result = TestMinorBackwardCompat();
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SchemaMigrationTest, PatchNegotiation)
{
    auto result = TestPatchNegotiation();
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SchemaMigrationTest, ComponentSkip)
{
    auto result = TestComponentSkip();
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

// ─── Soak Tests ─────────────────────────────────────────────────────────────
// Note: Soak tests run with reduced tick counts for CI feasibility.

TEST(SoakTest, NormalOperation)
{
    auto result = SoakNormalOperation(10000, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SoakTest, IntermittentDesync)
{
    auto result = SoakIntermittentDesync(10000, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SoakTest, HighChurn)
{
    auto result = SoakHighChurn(10000, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

TEST(SoakTest, BandwidthPressure)
{
    auto result = SoakBandwidthPressure(10000, 42);
    EXPECT_TRUE(result.success) << "Failed: " << result.failureDetail;
}

} // namespace SagaEngine::Client::Replication
