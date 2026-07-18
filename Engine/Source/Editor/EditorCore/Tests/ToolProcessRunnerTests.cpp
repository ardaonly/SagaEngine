// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Processes/ToolProcessRunner.h"

#include <gtest/gtest.h>

#ifndef _WIN32
TEST(ToolProcessRunnerTests, CapturesSeparateStreamsAndExitCode)
{
    SagaEditor::ToolProcessRequest request;
    request.executable = "/bin/sh";
    request.arguments = {"-c", "printf output; printf error >&2; exit 7"};
    const auto result = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_EQ(result.classification, SagaEditor::ToolProcessExitClassification::Failed);
    EXPECT_EQ(result.exitCode, 7);
    EXPECT_EQ(result.standardOutput, "output");
    EXPECT_EQ(result.standardError, "error");
}

TEST(ToolProcessRunnerTests, HeavyStandardErrorDoesNotDeadlock)
{
    SagaEditor::ToolProcessRequest request;
    request.executable = "/bin/sh";
    request.arguments = {"-c", "i=0; while [ $i -lt 20000 ]; do printf x >&2; i=$((i+1)); done"};
    request.outputLimitBytes = 4096;
    const auto result = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.standardError.size(), 4096U);
}

TEST(ToolProcessRunnerTests, TimeoutTerminatesProcessGroup)
{
    SagaEditor::ToolProcessRequest request;
    request.executable = "/bin/sh";
    request.arguments = {"-c", "sleep 30"};
    request.timeout = std::chrono::milliseconds(100);
    const auto result = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_EQ(result.classification, SagaEditor::ToolProcessExitClassification::TimedOut);
    EXPECT_LT(result.duration, std::chrono::seconds(3));
}

TEST(ToolProcessRunnerTests, CancellationForceKillsTermIgnoringProcessGroupWithinBound)
{
    SagaEditor::ToolProcessRequest request;
    request.executable = "/bin/sh";
    request.arguments = {"-c",
        "trap '' TERM; (trap '' TERM; while :; do sleep 1; done) & while :; do sleep 1; done"};
    request.timeout = std::chrono::minutes(5);
    request.cancellationRequested = [] { return true; };
    request.requestCancellation = [] {};
    request.cancellationGrace = std::chrono::milliseconds(50);
    request.terminationGrace = std::chrono::milliseconds(50);
    request.forceKillGrace = std::chrono::milliseconds(50);
    const auto result = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_EQ(result.classification, SagaEditor::ToolProcessExitClassification::Cancelled);
    EXPECT_EQ(result.shutdownClassification,
              SagaEditor::ToolProcessShutdownClassification::Forced);
    EXPECT_LT(result.duration, std::chrono::seconds(2));
}
#endif
