// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Scripting/SagaScriptLanguageServiceClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#ifndef _WIN32
#include <csignal>
#include <sys/types.h>
#endif

namespace
{
std::filesystem::path ProjectRoot()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("saga-language-service-client-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "Scripts");
    return root;
}
}

TEST(SagaScriptLanguageServiceClientTests, ReusesRealProcessForDocumentFeatures)
{
    const auto root = ProjectRoot();
    const auto path = root / "Scripts/Game.cs";
    const std::string source = "namespace Sample; public sealed class Game { public int Score { get; set; } }\n";
    std::ofstream(path, std::ios::binary) << source;

    SagaEditor::LanguageServiceProjectSnapshot snapshot;
    snapshot.projectSessionId = "client-integration";
    snapshot.projectSnapshotIdentity = "sha256:client-integration";
    snapshot.canonicalRoot = root;
    snapshot.projectId = "client-integration";
    snapshot.manifestHash = "sha256:manifest";
    snapshot.lockHash = "sha256:lock";

    SagaEditor::SagaScriptLanguageServiceClient client;
    ASSERT_TRUE(client.Start(snapshot)) << client.LastDiagnostic();
    const SagaEditor::LanguageServiceDocumentIdentity document{
        "document-game", path, 1};
    auto opened = client.OpenDocument(document, source);
    ASSERT_EQ(opened.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    EXPECT_TRUE(opened.get().Succeeded());

    auto symbols = client.Request("symbols", document);
    ASSERT_EQ(symbols.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    const auto response = symbols.get();
    ASSERT_TRUE(response.Succeeded()) << response.errorMessage;
    ASSERT_FALSE(response.symbols.empty());
    EXPECT_EQ(response.symbols.front().name, "Game");

    client.Stop();
    EXPECT_EQ(client.State(), SagaEditor::LanguageServiceState::Stopped);
    std::filesystem::remove_all(root);
}

#ifndef _WIN32
TEST(SagaScriptLanguageServiceClientTests, RestartsAndReplaysUnsavedDocumentAfterCrash)
{
    const auto root = ProjectRoot();
    const auto path = root / "Scripts/Recovery.cs";
    const std::string source = "namespace Sample; public sealed class Recovery { public int UnsavedValue; }\n";

    SagaEditor::LanguageServiceProjectSnapshot snapshot;
    snapshot.projectSessionId = "client-recovery";
    snapshot.projectSnapshotIdentity = "sha256:client-recovery";
    snapshot.canonicalRoot = root;
    snapshot.projectId = "client-recovery";

    SagaEditor::SagaScriptLanguageServiceClient client;
    ASSERT_TRUE(client.Start(snapshot)) << client.LastDiagnostic();
    const SagaEditor::LanguageServiceDocumentIdentity document{
        "document-recovery", path, 7};
    ASSERT_TRUE(client.OpenDocument(document, source).get().Succeeded());
    const auto originalProcess = client.ProcessId();
    ASSERT_NE(originalProcess, 0u);
    ASSERT_EQ(::kill(static_cast<pid_t>(originalProcess), SIGKILL), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline &&
           (client.State() != SagaEditor::LanguageServiceState::Ready ||
            client.ProcessId() == originalProcess))
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    ASSERT_EQ(client.State(), SagaEditor::LanguageServiceState::Ready)
        << client.LastDiagnostic();
    ASSERT_NE(client.ProcessId(), originalProcess);

    const auto symbols = client.Request("symbols", document).get();
    ASSERT_TRUE(symbols.Succeeded()) << symbols.errorMessage;
    ASSERT_FALSE(symbols.symbols.empty());
    EXPECT_EQ(symbols.symbols.front().name, "Recovery");
    client.Stop();
    std::filesystem::remove_all(root);
}

TEST(SagaScriptLanguageServiceClientTests, StopsAfterThreeRestartsInRollingWindow)
{
    const auto root = ProjectRoot();
    SagaEditor::LanguageServiceProjectSnapshot snapshot;
    snapshot.projectSessionId = "client-restart-limit";
    snapshot.projectSnapshotIdentity = "sha256:client-restart-limit";
    snapshot.canonicalRoot = root;
    snapshot.projectId = "client-restart-limit";

    SagaEditor::SagaScriptLanguageServiceClient client;
    ASSERT_TRUE(client.Start(snapshot)) << client.LastDiagnostic();
    for (int restart = 0; restart < 3; ++restart)
    {
        const auto process = client.ProcessId();
        ASSERT_NE(process, 0u);
        ASSERT_EQ(::kill(static_cast<pid_t>(process), SIGKILL), 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline &&
               (client.State() != SagaEditor::LanguageServiceState::Ready ||
                client.ProcessId() == process))
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        ASSERT_EQ(client.State(), SagaEditor::LanguageServiceState::Ready)
            << client.LastDiagnostic();
    }

    ASSERT_EQ(::kill(static_cast<pid_t>(client.ProcessId()), SIGKILL), 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline &&
           client.State() != SagaEditor::LanguageServiceState::Unavailable)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_EQ(client.State(), SagaEditor::LanguageServiceState::Unavailable);
    EXPECT_NE(client.LastDiagnostic().find("restart limit"), std::string::npos);
    client.Stop();
    std::filesystem::remove_all(root);
}
#endif
