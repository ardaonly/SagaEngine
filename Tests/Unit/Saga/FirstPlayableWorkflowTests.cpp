/// @file FirstPlayableWorkflowTests.cpp
/// @brief Focused Product Shell first-playable runner and report tests.

#include "RuntimeEvidenceReport.h"
#include "RuntimeEvidenceRunner.h"
#include "SagaAppConfig.h"

#include <QStandardPaths>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{

namespace fs = std::filesystem;
using namespace SagaProduct;

[[nodiscard]] fs::path Temp(const std::string& name)
{
    const fs::path path = fs::temp_directory_path() / name;
    fs::remove_all(path);
    fs::create_directories(path);
    return path;
}

void Write(const fs::path& path, const nlohmann::json& value)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    output << value.dump(2);
}

[[nodiscard]] fs::path CopyExecutable(const fs::path& source,
                                      const std::string& name)
{
    const fs::path destination = Temp("saga_process_" + name) / "SagaRuntime";
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    return destination;
}

[[nodiscard]] fs::path FindHostExecutable(const char* name)
{
    const QString path = QStandardPaths::findExecutable(QString::fromUtf8(name));
    if (path.isEmpty())
    {
        throw std::runtime_error(std::string("Required test executable not found: ") +
                                 name);
    }
    return path.toStdString();
}

[[nodiscard]] nlohmann::json CommonReport()
{
    return {
        {"status", "Passed"},
        {"project", {{"projectId", "starter-arena"}}},
        {"scene", {{"sceneId", "starter-arena-local-loop"}}},
        {"diagnostics", nlohmann::json::array()},
        {"nonClaims", {"No production readiness claim"}},
    };
}

[[nodiscard]] nlohmann::json GameplaySection()
{
    return {
        {"status", "Passed"},
        {"finalState", {
            {"score", 10}, {"pickupCollected", true},
            {"playerState", "powered"}}},
        {"pickup", {
            {"position", {{"x", 0.48}, {"y", 0.24}}},
            {"radius", 0.05}}},
        {"trigger", {
            {"mutationTick", 21},
            {"playerPosition", {{"x", 0.48}, {"y", 0.192}}}}},
        {"mutations", {
            {{"operation", "SetBool"}},
            {{"operation", "AddInt32"}},
            {{"operation", "SetString"}}}},
    };
}

TEST(RuntimeEvidenceReportTest, ParsesHeadlessSmokeContract)
{
    const fs::path reportPath = Temp("saga_first_playable_smoke_parser") /
        "report.json";
    auto json = CommonReport();
    json["loop"] = {{"status", "Passed"}, {"frames", 30}};
    Write(reportPath, json);

    const RuntimeEvidenceReport report = ParseRuntimeEvidenceReport(
        reportPath, "starter-arena-smoke");

    EXPECT_EQ(report.status, EvidenceStatus::Passed);
    EXPECT_EQ(report.capabilities.project, EvidenceStatus::Passed);
    EXPECT_TRUE(report.diagnostics.empty());
}

TEST(RuntimeEvidenceReportTest, ParsesVisibleGameplayContract)
{
    const fs::path reportPath = Temp("saga_first_playable_visible_parser") /
        "report.json";
    auto json = CommonReport();
    json["scriptLifecycle"] = {{"status", "Passed"}};
    json["gameplay"] = GameplaySection();
    json["input"] = {{"status", "Passed"}, {"source", "synthetic"}};
    json["simulation"] = {
        {"finalPosition", {{"x", 0.48}, {"y", 0.48}}}};
    json["render"] = {
        {"status", "Passed"}, {"requestedFrames", 30},
        {"presentedFrames", 30}, {"gameplayStateReflected", true},
        {"pickupDrawSubmittedLastFrame", false},
        {"poweredPlayerDrawSubmitted", true}};
    Write(reportPath, json);

    const RuntimeEvidenceReport report = ParseRuntimeEvidenceReport(
        reportPath, "starter-arena-visible-gameplay");

    EXPECT_EQ(report.status, EvidenceStatus::Passed);
    EXPECT_EQ(report.capabilities.gameplay, EvidenceStatus::Passed);
    EXPECT_EQ(report.capabilities.render, EvidenceStatus::Passed);
}

TEST(RuntimeEvidenceReportTest, RejectsTriggerOutsidePickupRadius)
{
    const fs::path reportPath = Temp("saga_first_playable_trigger_parser") /
        "report.json";
    auto json = CommonReport();
    json["scriptLifecycle"] = {{"status", "Passed"}};
    json["gameplay"] = GameplaySection();
    json["gameplay"]["trigger"]["playerPosition"] =
        {{"x", 1.0}, {"y", 1.0}};
    Write(reportPath, json);

    const RuntimeEvidenceReport report = ParseRuntimeEvidenceReport(
        reportPath, "starter-arena-gameplay");

    EXPECT_EQ(report.status, EvidenceStatus::Failed);
    ASSERT_FALSE(report.diagnostics.empty());
    EXPECT_NE(report.diagnostics.back().message.find("outside pickup radius"),
              std::string::npos);
}

TEST(RuntimeEvidenceReportTest, RejectsMalformedAndDiagnosticsBearingReports)
{
    const fs::path root = Temp("saga_first_playable_malformed_parser");
    const fs::path malformed = root / "malformed.json";
    std::ofstream(malformed) << "{";
    EXPECT_EQ(ParseRuntimeEvidenceReport(malformed, "starter-arena-smoke").status,
              EvidenceStatus::Failed);

    auto json = CommonReport();
    json["loop"] = {{"status", "Passed"}, {"frames", 30}};
    json["diagnostics"] = {{{"code", "Runtime.Error"}}};
    const fs::path diagnosed = root / "diagnosed.json";
    Write(diagnosed, json);
    EXPECT_EQ(ParseRuntimeEvidenceReport(diagnosed,
        "starter-arena-smoke").status, EvidenceStatus::Failed);
}

TEST(RuntimeEvidenceRunnerTest, BuildsBoundedProfileArgumentsWithoutRuntimeHeaders)
{
    RuntimeEvidenceRunResult prepared;
    prepared.projectManifest = "/source/StarterArena/StarterArena.sagaproj";
    prepared.executionProjectManifest =
        "/tmp/evidence/workspace/StarterArena.sagaproj";
    prepared.scriptManifest = "/tmp/evidence/script_bindings.json";
    prepared.scriptArtifacts = "/tmp/evidence/script_artifacts.json";

    const auto visible = RuntimeEvidenceRunner::BuildProfileArguments(
        RuntimeEvidenceProfile::StarterArenaVisibleGameplay,
        prepared, "/tmp/evidence/visible.json");
    const std::string joined = [&]() {
        std::string value;
        for (const auto& argument : visible) value += argument + " ";
        return value;
    }();

    EXPECT_NE(joined.find("--starter-arena-playable"), std::string::npos);
    EXPECT_NE(joined.find("--playable-input-source synthetic"), std::string::npos);
    EXPECT_NE(joined.find("--run-starter-arena-gameplay"), std::string::npos);
    EXPECT_NE(joined.find("--run-starter-arena-script-lifecycle"),
              std::string::npos);
}

TEST(RuntimeEvidenceRunnerTest, RejectsGeneratedOutputInsideProject)
{
    const fs::path root = Temp("saga_first_playable_output_policy");
    Write(root / "StarterArena.sagaproj",
          {{"projectId", "starter-arena"}});
    std::ofstream(root / "runtime") << "runtime";
    std::ofstream(root / "sagascript") << "script";
    std::ofstream(root / "bridge.dll") << "bridge";

    RuntimeEvidenceRunRequest request;
    request.projectManifest = root / "StarterArena.sagaproj";
    request.outputDirectory = root / "Build" / "Reports";
    request.runtimeExecutable = root / "runtime";
    request.sagaScriptExecutable = root / "sagascript";
    request.runtimeBridgeAssembly = root / "bridge.dll";

    RuntimeEvidenceRunner runner;
    const RuntimeEvidenceRunResult result = runner.Run(request);

    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.back().code,
              "ProductShell.FirstPlayable.GeneratedOutputInsideSourceTree");
}

TEST(RuntimeEvidenceProcessTest, CapturesOutputAndNonzeroExit)
{
    QtEvidenceProcessRunner runner;
    EvidenceProcessRequest request;
    request.executable = CopyExecutable(FindHostExecutable("ls"), "failure");
    request.arguments = {"/path/that/does/not/exist"};
    request.workingDirectory = request.executable.parent_path();
    request.timeout = std::chrono::milliseconds(1000);

    const EvidenceProcessResult result = runner.Run(request);

    EXPECT_TRUE(result.started);
    EXPECT_FALSE(result.timedOut);
    EXPECT_NE(result.exitCode, 0);
    EXPECT_FALSE(result.standardError.empty());
}

TEST(RuntimeEvidenceProcessTest, TerminatesAtTimeout)
{
#if !defined(__linux__)
    GTEST_SKIP() << "Self-executable timeout fixture currently uses /proc/self/exe.";
#else
    QtEvidenceProcessRunner runner;
    EvidenceProcessRequest request;
    request.executable = CopyExecutable(
        fs::read_symlink("/proc/self/exe"), "timeout");
    request.arguments = {
        "--gtest_filter=RuntimeEvidenceProcessHelper.SleepsLongEnoughForTimeout"};
    request.workingDirectory = request.executable.parent_path();
    request.timeout = std::chrono::milliseconds(10);

    const EvidenceProcessResult result = runner.Run(request);

    EXPECT_TRUE(result.started);
    EXPECT_TRUE(result.timedOut);
    EXPECT_LT(result.duration, std::chrono::milliseconds(900));
#endif
}

TEST(RuntimeEvidenceProcessHelper, SleepsLongEnoughForTimeout)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

TEST(FirstPlayableConfigTest, ParsesProductShellWorkflowOptions)
{
    std::vector<std::string> values = {
        "Saga", "--first-playable-check", "--project", "StarterArena.sagaproj",
        "--first-playable-output", "/tmp/evidence", "--runtime-executable",
        "/tmp/SagaRuntime", "--runtime-bridge-assembly", "/tmp/bridge.dll",
        "--first-playable-timeout-ms", "1234",
        "--first-playable-keyboard-report", "/tmp/keyboard.json"};
    std::vector<char*> argv;
    for (std::string& value : values) argv.push_back(value.data());

    const SagaConfigResult parsed = ParseSagaAppConfig(
        static_cast<int>(argv.size()), argv.data());

    ASSERT_TRUE(parsed.ok) << parsed.error;
    EXPECT_TRUE(parsed.config.firstPlayableCheck);
    EXPECT_EQ(parsed.config.firstPlayableTimeoutMs, 1234);
    EXPECT_EQ(parsed.config.firstPlayableOutputDirectory, "/tmp/evidence");
    EXPECT_EQ(parsed.config.firstPlayableKeyboardReportPath, "/tmp/keyboard.json");
}

TEST(FirstPlayableConfigTest, ParsesHumanCaptureOptionsAndRejectsModeConflict)
{
    std::vector<std::string> values = {"Saga", "--first-playable-human-capture",
        "--project", "StarterArena.sagaproj", "--first-playable-human-frames", "900",
        "--first-playable-human-timeout-ms", "45000"};
    std::vector<char*> argv;
    for (std::string& value : values) argv.push_back(value.data());
    auto parsed = ParseSagaAppConfig(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(parsed.ok) << parsed.error;
    EXPECT_TRUE(parsed.config.firstPlayableHumanCapture);
    EXPECT_EQ(parsed.config.firstPlayableHumanFrames, 900);
    EXPECT_EQ(parsed.config.firstPlayableHumanTimeoutMs, 45000);

    values.push_back("--first-playable-check");
    argv.clear();
    for (std::string& value : values) argv.push_back(value.data());
    parsed = ParseSagaAppConfig(static_cast<int>(argv.size()), argv.data());
    EXPECT_FALSE(parsed.ok);
}

TEST(FirstPlayableConfigTest, RejectsInvalidHumanCaptureBounds)
{
    for (const auto& arguments : std::vector<std::vector<std::string>>{
        {"Saga", "--first-playable-human-capture",
            "--first-playable-human-frames", "0"},
        {"Saga", "--first-playable-human-capture",
            "--first-playable-human-timeout-ms", "invalid"}})
    {
        auto values = arguments;
        std::vector<char*> argv;
        for (std::string& value : values) argv.push_back(value.data());
        const auto parsed = ParseSagaAppConfig(
            static_cast<int>(argv.size()), argv.data());
        EXPECT_FALSE(parsed.ok);
    }
}

} // namespace
