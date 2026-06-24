/// @file DiligentGpuTimelineTests.cpp
/// @brief CPU-side coverage for private Diligent GPU timeline bookkeeping.

#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"

#include <gtest/gtest.h>

namespace SagaEngine::Render::Backend
{
namespace
{

TEST(GpuTimeline, UninitializedTimelineAdvancesSubmissionSerials)
{
    DiligentGpuTimeline timeline;

    EXPECT_FALSE(timeline.IsInitialized());
    EXPECT_EQ(timeline.BeginFrameSubmission(), 1u);
    EXPECT_EQ(timeline.BeginFrameSubmission(), 2u);
    EXPECT_EQ(timeline.PollCompletion(), 0u);
    timeline.SignalFrameSubmitted(2u);
    timeline.WaitForSerial(2u);

    const auto diagnostics = timeline.Diagnostics();
    EXPECT_EQ(diagnostics.lastSubmittedSerial, 0u);
    EXPECT_EQ(diagnostics.lastCompletedSerial, 0u);
    EXPECT_EQ(diagnostics.targetedWaitCount, 0u);
    EXPECT_EQ(diagnostics.deviceWideWaitCount, 0u);
    EXPECT_EQ(diagnostics.pollCount, 1u);
    EXPECT_EQ(diagnostics.signalCount, 0u);
}

TEST(GpuTimeline, ResetClearsSerialsAndDiagnostics)
{
    DiligentGpuTimeline timeline;
    EXPECT_EQ(timeline.BeginFrameSubmission(), 1u);
    EXPECT_EQ(timeline.PollCompletion(), 0u);

    timeline.Reset();

    EXPECT_FALSE(timeline.IsInitialized());
    EXPECT_EQ(timeline.BeginFrameSubmission(), 1u);
    const auto diagnostics = timeline.Diagnostics();
    EXPECT_EQ(diagnostics.pollCount, 0u);
    EXPECT_EQ(diagnostics.lastSubmittedSerial, 0u);
}

} // namespace
} // namespace SagaEngine::Render::Backend
