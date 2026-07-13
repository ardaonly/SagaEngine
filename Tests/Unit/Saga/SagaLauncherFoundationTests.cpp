/// @file SagaLauncherFoundationTests.cpp
/// @brief Focused Product Launcher foundation contracts.

#include "Launcher/SagaLauncherController.h"
#include "SagaLauncherReports.h"
#include "SagaLauncherTargets.h"
#include "SagaProcessService.h"
#include "SagaProjectCatalog.h"
#include "SagaSessionModel.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <nlohmann/json.hpp>
#include <set>
#include <thread>

namespace
{
namespace fs = std::filesystem;
using namespace SagaProduct;

[[nodiscard]] fs::path Temp(const std::string& name)
{
    const auto root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void Write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    output << text;
}

[[nodiscard]] fs::path WriteProject(const fs::path& root,
                                    const std::string& id = "starter-arena",
                                    const std::string& name = "StarterArena")
{
    const auto manifest = root / (name + ".sagaproj");
    Write(manifest,
          nlohmann::json({{"schemaVersion", 0}, {"projectId", id}, {"displayName", name}}).dump(2));
    return manifest;
}

void MakeExecutable(const fs::path& path)
{
    fs::path source;
    for (const auto& candidate : {fs::path("/run/current-system/sw/bin/python3"),
                                  fs::path("/usr/bin/python3"),
                                  fs::path("/bin/python3")})
    {
        if (fs::is_regular_file(candidate))
        {
            source = candidate;
            break;
        }
    }
    if (source.empty())
        throw std::runtime_error("python executable is unavailable");
    fs::create_directories(path.parent_path());
    fs::copy_file(source, path, fs::copy_options::overwrite_existing);
    fs::permissions(path,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                        fs::perms::others_read | fs::perms::others_exec);
}

class ManualTaskRunner final : public ISagaLauncherTaskRunner
{
public:
    bool Start(std::function<void(std::stop_token)> value) override
    {
        if (running)
            return false;
        running = true;
        task = std::move(value);
        source = std::stop_source{};
        return true;
    }

    bool RequestStop() override { return source.request_stop(); }
    bool Running() const noexcept override { return running; }

    void Finish()
    {
        auto pending = std::move(task);
        if (pending)
            pending(source.get_token());
        running = false;
    }

    bool running = false;
    std::stop_source source;
    std::function<void(std::stop_token)> task;
};

class FakeExecutor final : public ISagaLauncherActionExecutor
{
public:
    SagaLauncherActionResult Execute(SagaLauncherActionId action,
                                     const SagaLauncherProjectSummary&,
                                     std::stop_token token) override
    {
        ++calls;
        SagaLauncherActionResult result;
        result.actionId = action;
        result.started = true;
        result.cancelled = token.stop_requested();
        result.status = result.cancelled ? SagaLauncherActionStatus::Cancelled : nextStatus;
        result.processExitClassification = result.cancelled
                                               ? SagaProcessExitClassification::Cancelled
                                               : SagaProcessExitClassification::Succeeded;
        return result;
    }

    int calls = 0;
    SagaLauncherActionStatus nextStatus = SagaLauncherActionStatus::Passed;
};

class EmptyReports final : public ISagaReportCatalog
{
public:
    std::vector<SagaLauncherReportReference> Read(
        const std::optional<SagaLauncherProjectSummary>&) const override
    {
        return {};
    }
};

class EmptyDistribution final : public ISagaDistributionStatusReader
{
public:
    SagaLauncherDistributionSummary Read() const override { return {}; }
};

struct ControllerFixture
{
    explicit ControllerFixture(const fs::path& root)
    {
        const auto product = root / "bin" / "Saga";
        MakeExecutable(product);
        MakeExecutable(root / "bin" / "SagaEditor");
        MakeExecutable(root / "bin" / "SagaRuntime");
        MakeExecutable(root / "bin" / "sagaproject");
        MakeExecutable(root / "bin" / "sagascript");

        auto executor = std::make_unique<FakeExecutor>();
        executorPtr = executor.get();
        auto task = std::make_unique<ManualTaskRunner>();
        taskPtr = task.get();
        controller = std::make_unique<SagaLauncherController>(
            std::make_unique<SagaProjectCatalog>(),
            std::make_unique<SagaRecentProjectsStore>(root / "config" / "recent_projects.json"),
            std::make_unique<SagaTargetResolver>(SagaExecutableCatalog(product)),
            std::move(executor),
            std::make_unique<EmptyReports>(),
            std::make_unique<EmptyDistribution>(),
            std::move(task));
        controller->LoadInitialState();
    }

    FakeExecutor* executorPtr = nullptr;
    ManualTaskRunner* taskPtr = nullptr;
    std::unique_ptr<SagaLauncherController> controller;
};

TEST(SagaLauncherModelTest, ExactActionTokensRoundTrip)
{
    const std::vector<std::string> tokens = {"open-project",
                                             "validate-project",
                                             "open-editor",
                                             "runtime-starter-arena-smoke",
                                             "runtime-starter-arena-playable",
                                             "first-playable-check",
                                             "refresh-reports",
                                             "view-sagascript-evidence",
                                             "view-visual-blocks-evidence",
                                             "view-package-status",
                                             "view-local-workspace-evidence",
                                             "unsupported-generic-runtime",
                                             "unsupported-server",
                                             "unsupported-world-server",
                                             "unsupported-cloud-collaboration"};
    for (const auto& token : tokens)
    {
        const auto parsed = ParseLauncherActionId(token);
        ASSERT_TRUE(parsed.has_value()) << token;
        EXPECT_EQ(ToString(*parsed), token);
    }
    EXPECT_FALSE(ParseLauncherActionId("run-arbitrary-executable").has_value());
    EXPECT_EQ(ToString(SagaLauncherActionStatus::Cancelled), std::string("cancelled"));
}

TEST(SagaProjectCatalogTest, OpensExplicitCanonicalSchemaZeroProject)
{
    const auto root = Temp("saga_launcher_catalog_explicit");
    const auto manifest = WriteProject(root);
    const auto before = fs::file_size(manifest);

    const auto project = SagaProjectCatalog{}.OpenProject(manifest);

    ASSERT_TRUE(project.valid);
    EXPECT_EQ(project.projectId, "starter-arena");
    EXPECT_EQ(project.schemaVersion, 0);
    EXPECT_EQ(project.canonicalManifestPath, fs::weakly_canonical(manifest));
    EXPECT_FALSE(project.legacyCompatibility);
    EXPECT_EQ(fs::file_size(manifest), before);
}

TEST(SagaProjectCatalogTest, DirectoryPrefersSagaprojAndRejectsAmbiguity)
{
    const auto root = Temp("saga_launcher_catalog_directory");
    Write(root / "saga.project.json",
          R"({"schemaVersion":1,"projectId":"legacy","displayName":"Legacy"})");
    (void)WriteProject(root);
    const auto preferred = SagaProjectCatalog{}.OpenProject(root);
    ASSERT_TRUE(preferred.valid);
    EXPECT_FALSE(preferred.legacyCompatibility);

    (void)WriteProject(root, "second", "Second");
    const auto ambiguous = SagaProjectCatalog{}.OpenProject(root);
    ASSERT_FALSE(ambiguous.valid);
    ASSERT_FALSE(ambiguous.diagnostics.empty());
    EXPECT_EQ(ambiguous.diagnostics.front().diagnosticId,
              SagaLauncherProjectDiagnostics::ManifestAmbiguous);
}

TEST(SagaProjectCatalogTest, LegacyFallbackIsExplicitlyCompatibilityOnly)
{
    const auto root = Temp("saga_launcher_catalog_legacy");
    Write(root / "saga.project.json",
          R"({"schemaVersion":1,"projectId":"legacy","displayName":"Legacy"})");
    const auto project = SagaProjectCatalog{}.OpenProject(root);
    ASSERT_TRUE(project.valid);
    EXPECT_TRUE(project.legacyCompatibility);
    ASSERT_FALSE(project.diagnostics.empty());
    EXPECT_EQ(project.diagnostics.front().severity, SagaLauncherDiagnosticSeverity::Warning);

    const auto explicitLegacy = SagaProjectCatalog{}.OpenProject(root / "saga.project.json");
    ASSERT_FALSE(explicitLegacy.valid);
    EXPECT_EQ(explicitLegacy.diagnostics.front().diagnosticId,
              SagaLauncherProjectDiagnostics::ManifestMissing);
}

TEST(SagaProjectCatalogTest, InvalidSchemaAndPrivatePathsAreRejected)
{
    const auto root = Temp("saga_launcher_catalog_invalid");
    const auto manifest = root / "Bad.sagaproj";
    Write(manifest, R"({"schemaVersion":1,"projectId":"bad","displayName":"Bad"})");
    const auto schema = SagaProjectCatalog{}.OpenProject(manifest);
    ASSERT_FALSE(schema.valid);
    EXPECT_EQ(schema.diagnostics.front().diagnosticId,
              SagaLauncherProjectDiagnostics::SchemaUnsupported);

    const auto privateManifest = WriteProject(root / ".saga-private");
    const auto privateResult = SagaProjectCatalog{}.OpenProject(privateManifest);
    ASSERT_FALSE(privateResult.valid);
    EXPECT_EQ(privateResult.diagnostics.front().diagnosticId,
              SagaLauncherProjectDiagnostics::PrivatePathRejected);

    SagaLauncherPathPolicy policy;
    policy.reportsRoot = root / ".saga-private" / "reports";
    std::string error;
    EXPECT_FALSE(
        policy
            .ActionReportDirectory(SagaProjectCatalog{}.OpenProject(WriteProject(root / "valid")),
                                   SagaLauncherActionId::ValidateProject,
                                   error)
            .has_value());
}

TEST(SagaRecentProjectsStoreTest, MigratesOnlyAfterSuccessfulRememberAndDeduplicates)
{
    const auto root = Temp("saga_launcher_recent_migration");
    const auto legacyRoot = root / "Legacy";
    Write(legacyRoot / "saga.project.json",
          R"({"schemaVersion":1,"projectId":"legacy","displayName":"Legacy"})");
    const auto storage = root / "config" / "recent_projects.json";
    Write(storage,
          nlohmann::json({{"recentProjects",
                           nlohmann::json::array(
                               {{{"displayName", "Legacy"}, {"root", legacyRoot.string()}}})}})
              .dump(2));
    const auto original = fs::file_size(storage);
    SagaRecentProjectsStore store(storage);
    ASSERT_EQ(store.Load().size(), 1u);
    EXPECT_EQ(fs::file_size(storage), original);

    const auto manifest = WriteProject(root / "StarterArena");
    const auto project = SagaProjectCatalog{}.OpenProject(manifest);
    std::string error;
    ASSERT_TRUE(store.Remember(project, error)) << error;
    ASSERT_TRUE(store.Remember(project, error)) << error;
    const auto recent = store.Load();
    ASSERT_EQ(recent.size(), 2u);
    EXPECT_EQ(recent.front().projectId, "starter-arena");
    nlohmann::json persisted;
    std::ifstream(storage) >> persisted;
    EXPECT_EQ(persisted["schemaVersion"], 1);
}

TEST(SagaTargetResolverTest, ExposesExactBoundedAndUnsupportedTargets)
{
    const auto root = Temp("saga_launcher_targets");
    const auto product = root / "bin" / "Saga";
    MakeExecutable(product);
    MakeExecutable(root / "bin" / "SagaEditor");
    MakeExecutable(root / "bin" / "SagaRuntime");
    MakeExecutable(root / "bin" / "sagaproject");
    const auto project = SagaProjectCatalog{}.OpenProject(WriteProject(root / "project"));
    SagaTargetResolver resolver{SagaExecutableCatalog(product)};

    const auto targets = resolver.ResolveTargets(project);
    ASSERT_EQ(targets.size(), 11u);
    EXPECT_EQ(targets[0].targetKind, SagaLauncherTargetKind::EditorTarget);
    EXPECT_EQ(targets[1].availability, SagaLauncherActionAvailability::Available);
    EXPECT_EQ(targets[7].status, SagaLauncherActionStatus::Unsupported);
    EXPECT_EQ(targets[8].diagnostics.front().diagnosticId,
              SagaProductDiagnostics::ServerExecutionUnsupported);
    EXPECT_TRUE(std::all_of(targets.begin() + 7, targets.end(), [](const auto& target) {
        return target.availability == SagaLauncherActionAvailability::Disabled;
    }));

    const auto actions = resolver.ResolveActions(project);
    const auto server = std::find_if(actions.begin(), actions.end(), [](const auto& action) {
        return action.id == SagaLauncherActionId::UnsupportedServer;
    });
    ASSERT_NE(server, actions.end());
    EXPECT_FALSE(server->processTargetId.has_value());
    EXPECT_FALSE(server->canRun);
}

TEST(SagaLauncherControllerTest, OpenSelectsWithoutLaunchingAndOneActionRuns)
{
    const auto root = Temp("saga_launcher_controller_open");
    ControllerFixture fixture(root);
    const auto manifest = WriteProject(root / "project");

    const auto opened = fixture.controller->OpenProject(manifest);
    ASSERT_TRUE(opened.accepted);
    EXPECT_EQ(fixture.executorPtr->calls, 0);
    ASSERT_TRUE(fixture.controller->GetState().selectedProject.has_value());

    const auto started = fixture.controller->RunAction(SagaLauncherActionId::OpenEditor);
    EXPECT_TRUE(started.accepted);
    EXPECT_EQ(started.status, SagaLauncherActionStatus::Running);
    const auto second = fixture.controller->RunAction(
        SagaLauncherActionId::RuntimeStarterArenaSmoke);
    EXPECT_FALSE(second.accepted);
    ASSERT_FALSE(second.diagnostics.empty());
    EXPECT_EQ(second.diagnostics.front().diagnosticId,
              SagaLauncherActionDiagnostics::AlreadyRunning);
    fixture.taskPtr->Finish();
    EXPECT_EQ(fixture.executorPtr->calls, 1);
    EXPECT_FALSE(fixture.controller->GetState().runningActionId.has_value());
}

TEST(SagaLauncherControllerTest, CancellationAndUnsupportedActionsStayTyped)
{
    const auto root = Temp("saga_launcher_controller_cancel");
    ControllerFixture fixture(root);
    ASSERT_TRUE(fixture.controller->OpenProject(WriteProject(root / "project")).accepted);
    ASSERT_TRUE(
        fixture.controller->RunAction(SagaLauncherActionId::RuntimeStarterArenaSmoke).accepted);
    EXPECT_TRUE(
        fixture.controller->CancelAction(SagaLauncherActionId::RuntimeStarterArenaSmoke).accepted);
    fixture.taskPtr->Finish();
    const auto state = fixture.controller->GetState();
    const auto action = std::find_if(
        state.actions.begin(), state.actions.end(), [](const auto& value) {
            return value.id == SagaLauncherActionId::RuntimeStarterArenaSmoke;
        });
    ASSERT_NE(action, state.actions.end());
    EXPECT_EQ(action->status, SagaLauncherActionStatus::Cancelled);

    const int calls = fixture.executorPtr->calls;
    const auto unsupported = fixture.controller->RunAction(SagaLauncherActionId::UnsupportedServer);
    EXPECT_FALSE(unsupported.accepted);
    EXPECT_EQ(fixture.executorPtr->calls, calls);
}

TEST(SagaReportCatalogTest, PreservesRawLimitationsVerificationAndMissingReports)
{
    const auto root = Temp("saga_launcher_reports");
    const auto project = SagaProjectCatalog{}.OpenProject(WriteProject(root / "project"));
    const auto reportsRoot = root / "launcher-reports";
    Write(
        reportsRoot / "starter-arena" / "runtime-starter-arena-smoke" / "runtime-smoke.json",
        R"({"schemaVersion":1,"status":"PassedWithLimitations","verified":false,"limitations":["bounded"]})");
    const auto evidenceRoot = root / "evidence";
    Write(evidenceRoot / "starter-arena" / "run-100" / "first_playable_gate.json",
          R"({"schemaVersion":1,"gateStatus":"AcceptedWithManualEvidencePending"})");
    const auto reports = SagaReportCatalog(reportsRoot, evidenceRoot).Read(project);
    ASSERT_FALSE(reports.empty());
    const auto smoke = std::find_if(reports.begin(), reports.end(), [](const auto& report) {
        return report.reportType == "runtime-smoke";
    });
    ASSERT_NE(smoke, reports.end());
    EXPECT_EQ(smoke->rawStatus, "PassedWithLimitations");
    EXPECT_EQ(smoke->mappedStatus, SagaLauncherActionStatus::PassedWithLimitations);
    ASSERT_TRUE(smoke->verified.has_value());
    EXPECT_FALSE(*smoke->verified);
    EXPECT_EQ(smoke->limitations, std::vector<std::string>({"bounded"}));
    EXPECT_FALSE(reports.front().exists);
    EXPECT_EQ(reports.front().mappedStatus, SagaLauncherActionStatus::NotConfigured);
    const auto gate = std::find_if(reports.begin(), reports.end(), [](const auto& report) {
        return report.reportType == "first-playable-gate";
    });
    ASSERT_NE(gate, reports.end());
    EXPECT_TRUE(gate->exists);
    EXPECT_EQ(gate->mappedStatus, SagaLauncherActionStatus::PassedWithLimitations);

    Write(reportsRoot / "starter-arena" / "runtime-starter-arena-smoke" / "runtime-smoke.json",
          R"({"schemaVersion":"one","status":"Passed"})");
    const auto malformed = SagaReportCatalog(reportsRoot, evidenceRoot).Read(project);
    const auto malformedSmoke = std::find_if(malformed.begin(),
                                             malformed.end(),
                                             [](const auto& report) {
                                                 return report.reportType == "runtime-smoke";
                                             });
    ASSERT_NE(malformedSmoke, malformed.end());
    EXPECT_EQ(malformedSmoke->mappedStatus, SagaLauncherActionStatus::Failed);
    EXPECT_EQ(malformedSmoke->rawStatus, "InvalidSchema");
    EXPECT_EQ(malformedSmoke->diagnosticCount, 1u);
}

TEST(SagaDistributionStatusReaderTest, EnforcesExactPackageIdentityReadOnly)
{
    const auto root = Temp("saga_launcher_distribution");
    const auto executable = root / "Saga" / "bin" / "Saga";
    MakeExecutable(executable);
    const auto missingOverride =
        SagaDistributionStatusReader(executable, root / "missing-report.json").Read();
    EXPECT_EQ(missingOverride.mappedStatus, SagaLauncherActionStatus::MissingInput);
    ASSERT_FALSE(missingOverride.diagnostics.empty());
    const auto buildInfo = root / "Saga" / "BUILD_INFO.json";
    Write(
        buildInfo,
        nlohmann::json(
            {{"schemaVersion", 2},
             {"status", "Passed"},
             {"verified", false},
             {"includedApplications",
              nlohmann::json::array(
                  {{{"name", "Saga"}}, {{"name", "SagaEditor"}}, {{"name", "SagaRuntime"}}})},
             {"includedPublicTools",
              nlohmann::json::array(
                  {{{"name", "sagapack"}}, {{"name", "sagaproject"}}, {{"name", "sagascript"}}})}})
            .dump(2));
    const auto valid = SagaDistributionStatusReader(executable).Read();
    EXPECT_EQ(valid.mappedStatus, SagaLauncherActionStatus::PassedWithLimitations);
    EXPECT_TRUE(valid.diagnostics.empty());

    auto invalidJson = nlohmann::json::parse(std::ifstream(buildInfo));
    invalidJson["includedApplications"].erase(invalidJson["includedApplications"].begin());
    Write(buildInfo, invalidJson.dump(2));
    const auto invalid = SagaDistributionStatusReader(executable).Read();
    EXPECT_EQ(invalid.mappedStatus, SagaLauncherActionStatus::Failed);
    ASSERT_FALSE(invalid.diagnostics.empty());
    EXPECT_EQ(invalid.diagnostics.front().diagnosticId,
              SagaLauncherReportDiagnostics::IdentityMismatch);

    Write(buildInfo, R"({"schemaVersion":"two","status":"Passed"})");
    const auto malformed = SagaDistributionStatusReader(executable).Read();
    EXPECT_EQ(malformed.mappedStatus, SagaLauncherActionStatus::Failed);
    ASSERT_FALSE(malformed.diagnostics.empty());
    EXPECT_EQ(malformed.diagnostics.front().diagnosticId, SagaLauncherReportDiagnostics::Invalid);
}

TEST(SagaProcessServiceTest, SagaProjectIdentityAndCancellationAreBounded)
{
    EXPECT_TRUE(SagaProcessService::IsExecutableAllowed(SagaProcessTargetId::SagaProject,
                                                        "/opt/Saga/bin/sagaproject"));
    EXPECT_FALSE(
        SagaProcessService::IsExecutableAllowed(SagaProcessTargetId::SagaProject, "/bin/sh"));

    const auto root = Temp("saga_launcher_process_cancel");
    const auto runtime = root / "SagaRuntime";
    MakeExecutable(runtime);
    std::stop_source source;
    SagaProductProcessRequest request;
    request.target = SagaProcessTargetId::Runtime;
    request.executable = runtime;
    request.arguments = {"-c", "import time; time.sleep(5)"};
    request.workingDirectory = root;
    request.timeout = std::chrono::seconds(10);
    request.stopToken = source.get_token();
    auto future = std::async(std::launch::async, [&request]() {
        SagaProcessService service;
        return service.Run(request);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    source.request_stop();
    const auto result = future.get();
    EXPECT_TRUE(result.started);
    EXPECT_TRUE(result.cancelled);
    EXPECT_EQ(result.classification, SagaProcessExitClassification::Cancelled);
    EXPECT_LT(result.duration, std::chrono::seconds(3));
}

} // namespace
