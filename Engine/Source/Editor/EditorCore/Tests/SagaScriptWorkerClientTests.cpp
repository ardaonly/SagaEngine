// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/SagaScriptWorkerClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#ifndef _WIN32
namespace
{
namespace fs = std::filesystem;

fs::path Root(std::string_view name)
{
    const auto root = fs::temp_directory_path() / ("saga-worker-client-" +
        std::string(name) + "-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    return root;
}

fs::path Worker(const fs::path& root, std::string_view operationId,
                std::string_view revision)
{
    const auto script = root / "worker.sh";
    std::ofstream output(script, std::ios::binary);
    output << "#!/bin/sh\n"
              "result=\nprogress=\n"
              "while [ $# -gt 0 ]; do\n"
              "  if [ \"$1\" = \"--result\" ]; then result=$2; shift 2; continue; fi\n"
              "  if [ \"$1\" = \"--progress\" ]; then progress=$2; shift 2; continue; fi\n"
              "  shift\n"
              "done\n"
              "printf '%s\\n' '{\"schemaVersion\":1,\"stage\":\"Analyzing\"}' > \"$progress\"\n"
              "printf '%s\\n' '{\"schemaVersion\":1,\"tool\":\"sagascript\",\"status\":\"Passed\",\"failureClass\":\"\",\"cancelled\":false,\"stale\":false,\"operationId\":\""
           << operationId << "\",\"sourceRevision\":\"" << revision
           << "\",\"currentManifestHash\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"diagnostics\":[]}' > \"$result\"\n";
    output.close();
    fs::permissions(script, fs::perms::owner_exec | fs::perms::owner_read |
        fs::perms::owner_write, fs::perm_options::replace);
    return script;
}

SagaEditor::ScriptWorkerRequest Request(const fs::path& root)
{
    SagaEditor::ScriptWorkerRequest request;
    request.operationId = "operation-1";
    request.projectId = "project-1";
    request.project = {root / "Project.sagaproj", "sha256:project"};
    request.sourceRevision = "revision-1";
    request.outputRoot = root / "output";
    request.transportRoot = root / "transport";
    return request;
}
}

TEST(SagaScriptWorkerClientTests, ParsesIdentityAndProgressFromWorkerProtocol)
{
    const auto root = Root("passed");
    auto request = Request(root);
    std::vector<std::string> progress;
    request.progress = [&progress](std::string_view stage) { progress.emplace_back(stage); };
    const auto result = SagaEditor::SagaScriptWorkerClient(
        Worker(root, request.operationId, request.sourceRevision)).Run(request);
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.progressStages, (std::vector<std::string>{"Analyzing"}));
    EXPECT_EQ(progress, (std::vector<std::string>{"Analyzing"}));
    fs::remove_all(root);
}

TEST(SagaScriptWorkerClientTests, RejectsStaleOperationIdentity)
{
    const auto root = Root("stale");
    const auto request = Request(root);
    const auto result = SagaEditor::SagaScriptWorkerClient(
        Worker(root, "old-operation", request.sourceRevision)).Run(request);
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.stale);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.back().code, "Script.Worker.ResultIdentityMismatch");
    fs::remove_all(root);
}
#endif
