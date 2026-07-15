/// @file FirstPlayableHumanCaptureTests.cpp
/// @brief Focused operator-workflow tests for real-keyboard evidence capture.

#include "FirstPlayable/FirstPlayableHumanCapture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
namespace fs = std::filesystem;
using namespace SagaProduct;
using Json = nlohmann::json;

void Text(const fs::path& path, const std::string& value)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << value;
}

void Write(const fs::path& path, const Json& value)
{
    Text(path, value.dump(2) + "\n");
}

Json Read(const fs::path& path)
{
    std::ifstream input(path);
    Json value;
    input >> value;
    return value;
}

Json KeyboardReport()
{
    return {{"diagnostics", Json::array()},
        {"input", {{"source", "keyboard"}, {"status", "Passed"},
            {"realDeviceObserved", true}, {"framesWithInput", 3}}},
        {"simulation", {{"initialPosition", {{"x", 0.0}, {"y", 0.0}}},
            {"finalPosition", {{"x", 0.2}, {"y", 0.0}}}}}};
}

fs::path Fixture(const char* name)
{
    const auto root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    Write(root / "project" / "StarterArena.sagaproj", {{"projectId", "starter-arena"}});
    Write(root / "project" / "Scenes" / "arena.scene.json", Json::object());
    Text(root / "project" / "Scripts" / "GameRules.cs", "class GameRules {}\n");
    Write(root / "project" / "Input" / "playable.synthetic-input.json", Json::object());
    std::string claims;
    for (const auto& claim : FirstPlayablePublicClaimAudit::CanonicalNonClaims())
        claims += "- " + claim + "\n";
    Text(root / "project" / "README.md", claims);
    Text(root / "project" / "ACCEPTANCE.md", "Exact evidence only.\n");
    Text(root / "project" / "KNOWN_LIMITATIONS.md", "Bounded evidence.\n");
    fs::create_directories(root / ".git");
    Text(root / "bridge.dll", "bridge");
    return root;
}

FirstPlayableHumanCaptureRequest Request(const fs::path& root,
                                         const fs::path& output)
{
    FirstPlayableHumanCaptureRequest request;
    request.workflow.runtime.projectManifest =
        root / "project" / "StarterArena.sagaproj";
    request.workflow.runtime.outputDirectory = output;
    request.workflow.runtime.runtimeExecutable = "/bin/sh";
    request.workflow.runtime.sagaScriptExecutable = "/bin/sh";
    request.workflow.runtime.runtimeBridgeAssembly = root / "bridge.dll";
    request.workflow.summaryPath = output / "first_playable_summary.json";
    request.frames = 42;
    request.timeout = std::chrono::milliseconds(1234);
    return request;
}

class FakeCaptureRunner final : public IEvidenceProcessRunner
{
public:
    explicit FakeCaptureRunner(EvidenceProcessResult result, bool writeReport = false)
        : result_(std::move(result)), writeReport_(writeReport)
    {}

    EvidenceProcessResult Run(const EvidenceProcessRequest& request) override
    {
        ++calls;
        lastRequest = request;
        if (writeReport_)
        {
            for (std::size_t index = 0; index + 1 < request.arguments.size(); ++index)
                if (request.arguments[index] == "--playable-report-out")
                    Write(request.arguments[index + 1], KeyboardReport());
        }
        return result_;
    }

    int calls = 0;
    EvidenceProcessRequest lastRequest;

private:
    EvidenceProcessResult result_;
    bool writeReport_ = false;
};

TEST(FirstPlayableHumanCaptureTest, BuildsExistingBoundedKeyboardRuntimeContract)
{
    const auto root = Fixture("saga_human_capture_arguments");
    const auto output = fs::temp_directory_path() / "saga_human_capture_arguments_out";
    const auto request = Request(root, output);
    const auto process = FirstPlayableHumanCapture::BuildProcessRequest(
        request, "/tmp/capture.json");
    std::string joined;
    for (const auto& argument : process.arguments) joined += argument + " ";
    EXPECT_NE(joined.find("--starter-arena-playable"), std::string::npos);
    EXPECT_NE(joined.find("--playable-input-source keyboard"), std::string::npos);
    EXPECT_NE(joined.find("--playable-frames 42"), std::string::npos);
    EXPECT_NE(joined.find("--fixed-dt 0.016"), std::string::npos);
    EXPECT_EQ(process.timeout, std::chrono::milliseconds(1234));
}

TEST(FirstPlayableHumanCaptureTest, LaunchFailureIsIncompleteAndWritesOperatorReport)
{
    const auto root = Fixture("saga_human_capture_launch_failure");
    const auto output = fs::temp_directory_path() / "saga_human_capture_launch_failure_out";
    fs::remove_all(output);
    EvidenceProcessResult process;
    process.startError = "not started";
    auto runner = std::make_unique<FakeCaptureRunner>(process);
    auto* observed = runner.get();
    FirstPlayableHumanCapture capture(std::move(runner));
    std::ostringstream out, err;
    const auto result = capture.Run(Request(root, output), out, err);
    EXPECT_EQ(observed->calls, 1);
    EXPECT_EQ(result.status, EvidenceStatus::Incomplete);
    EXPECT_EQ(result.gateStatus, FirstPlayableGateStatus::Incomplete);
    EXPECT_TRUE(fs::is_regular_file(
        output / "manual" / "human_capture_report.json"));
}

TEST(FirstPlayableHumanCaptureTest, ValidCaptureIsImportedBeforeAuthoritativeGateRuns)
{
    const auto root = Fixture("saga_human_capture_valid_import");
    const auto output = fs::temp_directory_path() / "saga_human_capture_valid_import_out";
    fs::remove_all(output);
    EvidenceProcessResult process;
    process.started = true;
    process.exitCode = 0;
    process.standardOutput = "capture out";
    auto runner = std::make_unique<FakeCaptureRunner>(process, true);
    FirstPlayableHumanCapture capture(std::move(runner));
    std::ostringstream out, err;
    const auto result = capture.Run(Request(root, output), out, err);
    EXPECT_EQ(result.keyboardEvidence.status, EvidenceStatus::Passed);
    EXPECT_TRUE(fs::is_regular_file(
        output / "manual" / "real_keyboard_capture_report.json"));
    EXPECT_TRUE(fs::is_regular_file(
        output / "manual" / "real_keyboard_report.json"));
    EXPECT_TRUE(fs::is_regular_file(
        output / "manual" / "human_capture_report.json"));
    const Json gate = Read(output / "first_playable_gate.json");
    const Json keyboard = gate["gate"]["manualEvidence"]["realKeyboard"];
    EXPECT_EQ(keyboard["status"], "Passed");
    EXPECT_EQ(keyboard["reportPath"], "manual/real_keyboard_report.json");
    EXPECT_EQ(keyboard["captureReportPath"],
              "manual/real_keyboard_capture_report.json");
    EXPECT_EQ(keyboard["sha256"], result.keyboardEvidence.reportSha256);
    ASSERT_TRUE(keyboard["originalReportPath"].is_string());
    EXPECT_NE(keyboard["originalReportPath"], keyboard["reportPath"]);
}

TEST(FirstPlayableHumanCaptureTest, TimeoutIsIncompleteRatherThanAcceptedPending)
{
    const auto root = Fixture("saga_human_capture_timeout");
    const auto output = fs::temp_directory_path() / "saga_human_capture_timeout_out";
    fs::remove_all(output);
    EvidenceProcessResult process;
    process.started = true;
    process.timedOut = true;
    process.exitCode = 1;
    auto runner = std::make_unique<FakeCaptureRunner>(process);
    FirstPlayableHumanCapture capture(std::move(runner));
    std::ostringstream out, err;
    const auto result = capture.Run(Request(root, output), out, err);
    EXPECT_EQ(result.status, EvidenceStatus::Incomplete);
    EXPECT_EQ(result.gateStatus, FirstPlayableGateStatus::Incomplete);
    EXPECT_NE(err.str().find("ProductShell.HumanCapture.ProcessTimedOut"),
              std::string::npos);
}

TEST(FirstPlayableHumanCaptureTest, ImportModeNeverUsesCaptureProcessRunner)
{
    const auto root = Fixture("saga_human_capture_import_mode");
    const auto output = fs::temp_directory_path() / "saga_human_capture_import_mode_out";
    fs::remove_all(output);
    const auto keyboard = root / "keyboard.json";
    Write(keyboard, KeyboardReport());
    EvidenceProcessResult unused;
    auto runner = std::make_unique<FakeCaptureRunner>(unused);
    auto* observed = runner.get();
    FirstPlayableHumanCapture capture(std::move(runner));
    auto request = Request(root, output);
    request.workflow.keyboardReportPath = keyboard;
    std::ostringstream out, err;
    const auto result = capture.Run(request, out, err);
    EXPECT_EQ(observed->calls, 0);
    EXPECT_EQ(result.mode, FirstPlayableOperatorMode::Import);
    EXPECT_TRUE(fs::is_regular_file(
        output / "manual" / "real_keyboard_report.json"));
    EXPECT_FALSE(fs::exists(
        output / "manual" / "real_keyboard_capture_report.json"));
    const Json gate = Read(output / "first_playable_gate.json");
    const Json packaged = gate["gate"]["manualEvidence"]["realKeyboard"];
    EXPECT_EQ(packaged["reportPath"], "manual/real_keyboard_report.json");
    EXPECT_EQ(packaged["originalReportPath"], fs::absolute(keyboard).string());
    EXPECT_TRUE(packaged["captureReportPath"].is_null());
}

TEST(FirstPlayableHumanCaptureBoundaryTest, SourceExcludesForbiddenDependencies)
{
    std::ifstream input(fs::path(SAGA_SOURCE_ROOT) /
        "Tests/Evidence/FirstPlayable/Source/FirstPlayableHumanCapture.cpp");
    const std::string source((std::istreambuf_iterator<char>(input)), {});
    for (const char* forbidden : {"SDE::", ".sde", "Prism", "ClientHost",
        "SagaEngine/ServerAuthority", "SagaEngine/Replication",
        "InputCommand", "Replication", "Prediction", "Network"})
        EXPECT_EQ(source.find(forbidden), std::string::npos) << forbidden;
}
} // namespace
