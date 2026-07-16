/// @file DiligentFrameSlotTrackerTests.cpp
/// @brief CPU-side coverage for private Diligent frame-slot ownership.

#include "SagaEngine/Render/Backend/Diligent/DiligentFrameSlotTracker.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace SagaEngine::Render::Backend
{
namespace
{

static_assert(!std::is_copy_constructible_v<DiligentFrameSlotTracker>);
static_assert(!std::is_copy_assignable_v<DiligentFrameSlotTracker>);
static_assert(!std::is_move_constructible_v<DiligentFrameSlotTracker>);
static_assert(!std::is_move_assignable_v<DiligentFrameSlotTracker>);

TEST(DiligentFrameSlotTracker, InvalidConfigurationRejectsFrameBegins)
{
    DiligentFrameSlotTracker tracker;

    EXPECT_FALSE(tracker.Configure(0u));
    const auto begin = tracker.BeginFrame(0u, 0u);

    EXPECT_FALSE(begin.valid);
    EXPECT_EQ(tracker.FrameSlotCount(), 0u);
}

TEST(DiligentFrameSlotTracker, ActiveFrameRejectsSecondBegin)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(2u, 64u));

    const auto first = tracker.BeginFrame(0u, 0u);
    ASSERT_TRUE(first.valid);
    EXPECT_EQ(first.frameSlot, 0u);
    EXPECT_TRUE(tracker.ReserveActiveBytes(32u));
    tracker.BeginSubmission(9u);

    const auto second = tracker.BeginFrame(1u, 0u);

    EXPECT_FALSE(second.valid);
    EXPECT_TRUE(tracker.ActiveFrameOpen());
    EXPECT_EQ(tracker.ActiveFrameSlot(), 0u);
    EXPECT_EQ(tracker.ActiveFrameSerial(), 9u);
    EXPECT_EQ(tracker.SlotSerial(0u), 0u);
    EXPECT_EQ(tracker.Diagnostics().activeSlotUsedBytes, 32u);
    EXPECT_EQ(tracker.Diagnostics().doubleBeginRejectCount, 1u);

    tracker.EndFrame();

    EXPECT_FALSE(tracker.ActiveFrameOpen());
    EXPECT_EQ(tracker.SlotSerial(0u), 9u);
    EXPECT_EQ(tracker.SlotSerial(1u), 0u);
}

TEST(DiligentFrameSlotTracker, FrameSlotProgressionUsesFrameIndexModulo)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(3u));

    EXPECT_EQ(tracker.BeginFrame(0u, 0u).frameSlot, 0u);
    tracker.BeginSubmission(1u);
    tracker.EndFrame();

    EXPECT_EQ(tracker.BeginFrame(1u, 1u).frameSlot, 1u);
    tracker.BeginSubmission(2u);
    tracker.EndFrame();

    EXPECT_EQ(tracker.BeginFrame(2u, 2u).frameSlot, 2u);
    tracker.BeginSubmission(3u);
    tracker.EndFrame();

    EXPECT_EQ(tracker.BeginFrame(3u, 3u).frameSlot, 0u);
}

TEST(DiligentFrameSlotTracker, InvalidBeginDoesNotConsumeTimelineSerial)
{
    DiligentFrameSlotTracker tracker;
    DiligentGpuTimeline timeline;
    ASSERT_TRUE(tracker.Configure(1u));

    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    const auto firstSerial = timeline.BeginFrameSubmission();
    EXPECT_EQ(firstSerial, 1u);
    tracker.BeginSubmission(firstSerial);

    const auto rejected = tracker.BeginFrame(1u, 0u);
    EXPECT_FALSE(rejected.valid);
    if (rejected.valid)
    {
        tracker.BeginSubmission(timeline.BeginFrameSubmission());
    }

    EXPECT_EQ(timeline.BeginFrameSubmission(), 2u);
}

TEST(DiligentFrameSlotTracker, IncompleteSlotReuseRequiresWait)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u));

    auto first = tracker.BeginFrame(0u, 0u);
    ASSERT_TRUE(first.resetEligible);
    tracker.BeginSubmission(7u);
    tracker.EndFrame();

    const auto second = tracker.BeginFrame(1u, 6u);

    EXPECT_TRUE(second.valid);
    EXPECT_FALSE(second.resetEligible);
    EXPECT_TRUE(second.waitRequired);
    EXPECT_EQ(second.waitSerial, 7u);
    EXPECT_EQ(tracker.Diagnostics().incompleteSlotReuseCount, 1u);
}

TEST(DiligentFrameSlotTracker, CompletedWaitMakesSlotReusable)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u));

    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    tracker.BeginSubmission(4u);
    tracker.EndFrame();

    const auto reuse = tracker.BeginFrame(1u, 3u);
    ASSERT_TRUE(reuse.waitRequired);
    tracker.MarkWaitCompleted(4u);

    const auto diagnostics = tracker.Diagnostics();
    EXPECT_EQ(diagnostics.lastCompletedSerial, 4u);
    EXPECT_EQ(diagnostics.resetCount, 2u);
    EXPECT_EQ(diagnostics.activeSlotUsedBytes, 0u);
}

TEST(DiligentFrameSlotTracker, CompletedSlotReuseResetsUsage)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u, 64u));

    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    EXPECT_TRUE(tracker.ReserveActiveBytes(32u));
    tracker.BeginSubmission(1u);
    tracker.EndFrame();

    const auto reuse = tracker.BeginFrame(1u, 1u);

    EXPECT_TRUE(reuse.resetEligible);
    EXPECT_FALSE(reuse.waitRequired);
    EXPECT_EQ(tracker.Diagnostics().activeSlotUsedBytes, 0u);
}

TEST(DiligentFrameSlotTracker, EndFrameWithoutBeginIsNoOp)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(2u));

    tracker.EndFrame();

    EXPECT_FALSE(tracker.ActiveFrameOpen());
    EXPECT_EQ(tracker.ActiveFrameSerial(), 0u);
    EXPECT_EQ(tracker.LatestSubmittedSerial(), 0u);
    EXPECT_EQ(tracker.SlotSerial(0u), 0u);
    EXPECT_EQ(tracker.SlotSerial(1u), 0u);
    EXPECT_EQ(tracker.Diagnostics().resetCount, 0u);
    EXPECT_EQ(tracker.Diagnostics().incompleteSlotReuseCount, 0u);
    EXPECT_EQ(tracker.Diagnostics().doubleBeginRejectCount, 0u);
}

TEST(DiligentFrameSlotTracker, DoubleEndFrameIsNoOpAfterFirstCommit)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u));

    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    tracker.BeginSubmission(5u);
    tracker.EndFrame();
    ASSERT_EQ(tracker.SlotSerial(0u), 5u);
    ASSERT_EQ(tracker.LatestSubmittedSerial(), 5u);

    tracker.EndFrame();

    EXPECT_FALSE(tracker.ActiveFrameOpen());
    EXPECT_EQ(tracker.ActiveFrameSerial(), 0u);
    EXPECT_EQ(tracker.SlotSerial(0u), 5u);
    EXPECT_EQ(tracker.LatestSubmittedSerial(), 5u);
}

TEST(DiligentFrameSlotTracker, ResetClearsSubmittedSerialsAndDiagnostics)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(2u));

    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    tracker.BeginSubmission(11u);
    tracker.EndFrame();
    ASSERT_EQ(tracker.LatestSubmittedSerial(), 11u);

    tracker.Reset();

    EXPECT_EQ(tracker.FrameSlotCount(), 0u);
    EXPECT_EQ(tracker.LatestSubmittedSerial(), 0u);
    EXPECT_EQ(tracker.Diagnostics().resetCount, 0u);
}

TEST(DiligentFrameSlotTracker, ReconfigureClearsResizeSlotOwnership)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(2u));

    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    tracker.BeginSubmission(5u);
    tracker.EndFrame();
    ASSERT_EQ(tracker.LatestSubmittedSerial(), 5u);

    ASSERT_TRUE(tracker.Configure(3u));

    EXPECT_EQ(tracker.FrameSlotCount(), 3u);
    EXPECT_EQ(tracker.LatestSubmittedSerial(), 0u);
    EXPECT_EQ(tracker.SlotSerial(0u), 0u);
}

TEST(DiligentFrameSlotTracker, ActiveMappingStateIsExclusive)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u));
    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);

    EXPECT_TRUE(tracker.BeginActiveMapping());
    EXPECT_FALSE(tracker.BeginActiveMapping());
    EXPECT_TRUE(tracker.Diagnostics().activeMapping);

    tracker.EndActiveMapping();
    EXPECT_FALSE(tracker.Diagnostics().activeMapping);
    EXPECT_TRUE(tracker.BeginActiveMapping());
}

TEST(DiligentFrameSlotTracker, BudgetOverflowIsRejectedAndRecorded)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u, 16u));
    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);

    EXPECT_TRUE(tracker.ReserveActiveBytes(8u));
    EXPECT_FALSE(tracker.ReserveActiveBytes(9u));

    const auto diagnostics = tracker.Diagnostics();
    EXPECT_EQ(diagnostics.activeSlotUsedBytes, 8u);
    EXPECT_EQ(diagnostics.overflowCount, 1u);
    EXPECT_TRUE(diagnostics.activeSlotOverflowed);
}

TEST(DiligentFrameSlotTracker, SerialComparisonDoesNotUseOverflowingSubtraction)
{
    DiligentFrameSlotTracker tracker;
    ASSERT_TRUE(tracker.Configure(1u));

    constexpr auto kNearMax =
        std::numeric_limits<std::uint64_t>::max() - 2u;
    ASSERT_TRUE(tracker.BeginFrame(0u, 0u).valid);
    tracker.BeginSubmission(kNearMax);
    tracker.EndFrame();

    const auto incomplete = tracker.BeginFrame(1u, kNearMax - 1u);
    EXPECT_TRUE(incomplete.waitRequired);
    EXPECT_EQ(incomplete.waitSerial, kNearMax);

    tracker.MarkWaitCompleted(kNearMax);
    EXPECT_EQ(tracker.Diagnostics().lastCompletedSerial, kNearMax);
}

} // namespace
} // namespace SagaEngine::Render::Backend
