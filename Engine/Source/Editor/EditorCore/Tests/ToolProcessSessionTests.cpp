// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Processes/ToolProcessSession.h"

#include <gtest/gtest.h>
#include <thread>

TEST(ToolProcessSessionTests, ExchangesMultipleMessagesAndReapsOnShutdown)
{
#ifdef _WIN32
    const auto executable = std::filesystem::path{"cmd.exe"};
    const std::vector<std::string> arguments{"/C", "for /L %i in (1,1,2) do @set /p line= & @echo %line%"};
#else
    const auto executable = std::filesystem::path{"/bin/sh"};
    const std::vector<std::string> arguments{"-c", "while IFS= read -r line; do printf '%s\\n' \"$line\"; done"};
#endif
    SagaEditor::ToolProcessSessionOptions options;
    options.process.executable = executable;
    options.process.arguments = arguments;
    SagaEditor::ToolProcessSession session;
    ASSERT_TRUE(session.Start(options)) << session.Diagnostic();
    ASSERT_TRUE(session.WriteLine("first"));
    ASSERT_TRUE(session.WriteLine("second"));
    EXPECT_EQ(session.ReadLine(std::chrono::seconds{2}), "first");
    EXPECT_EQ(session.ReadLine(std::chrono::seconds{2}), "second");
    session.Shutdown();
    EXPECT_NE(session.State(), SagaEditor::ToolProcessSessionState::Running);
}

TEST(ToolProcessSessionTests, RejectsOversizedOutboundMessage)
{
    SagaEditor::ToolProcessSessionOptions options;
#ifdef _WIN32
    options.process.executable = "cmd.exe";
    options.process.arguments = {"/C", "more"};
#else
    options.process.executable = "/bin/sh";
    options.process.arguments = {"-c", "cat"};
#endif
    options.messageLimitBytes = 8;
    SagaEditor::ToolProcessSession session;
    ASSERT_TRUE(session.Start(options));
    EXPECT_FALSE(session.WriteLine("message-too-large"));
    session.Shutdown();
}

#ifndef _WIN32
TEST(ToolProcessSessionTests, BrokenInputPipeFailsWithoutTerminatingHost)
{
    SagaEditor::ToolProcessSessionOptions options;
    options.process.executable = "/bin/sh";
    options.process.arguments = {"-c", "exec 0<&-; sleep 1"};
    SagaEditor::ToolProcessSession session;
    ASSERT_TRUE(session.Start(options));
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    EXPECT_FALSE(session.WriteLine("closed-input"));
    session.Shutdown();
}
#endif
