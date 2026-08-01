// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackageWorkflowController.h"

#include "SagaEditor/Diagnostics/EditorDiagnosticsService.h"
#include "SagaEditor/Documents/SourceDocuments.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace
{
namespace fs = std::filesystem;
using namespace SagaEditor;

std::string Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

class ProjectClient final : public IProjectToolClient
{
public:
    explicit ProjectClient(fs::path root) : root_(std::move(root)) {}
    ProjectToolResult Validate(const fs::path&) const override
    {
        ProjectToolResult result;
        result.passed = true;
        result.project.projectId = "package-controller-project";
        result.project.projectRoot = root_;
        result.project.manifestPath = root_ / "Fixture.sagaproj";
        return result;
    }
    ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest&) const override { return {}; }
private:
    fs::path root_;
};

class Adapter final : public IPackageWorkflowToolAdapter
{
public:
    explicit Adapter(fs::path root) : root_(std::move(root)) {}

    std::pair<PackageWorkflowCapture, PackageWorkflowResult> Capture(
        const EditorDocumentWorkspace&) const override
    {
        PackageWorkflowCapture capture;
        capture.projectRoot = root_;
        capture.manifestPath = root_ / "Fixture.sagaproj";
        capture.lockPath = root_ / "SagaPackage.lock";
        capture.manifestBytes = Read(capture.manifestPath);
        capture.lockBytes = Read(capture.lockPath);
        capture.sourceInventoryHash = "sha256:sources";
        capture.generationId = "generation";
        capture.generationHash = "sha256:generation";
        capture.graphHash = "sha256:graph";
        capture.ownershipCatalogHash = "sha256:ownership";
        capture.projectHash = "sha256:project";
        capture.manifestHash = HashDocumentBytes(capture.manifestBytes);
        capture.lockHash = HashDocumentBytes(capture.lockBytes);
        capture.generationManifestHash = "sha256:generation-manifest";
        capture.referenceInventoryHash = "sha256:references";
        return {capture, {PackageWorkflowStatus::Prepared, {}, {}, {}}};
    }

    PackageCandidateResolution ResolveCandidate(const PackageWorkflowCapture&,
        const fs::path&, const fs::path&,
        const PackageToolOperationContext&) const override
    {
        ++resolveCalls;
        return {true, {}, {}, "candidate-lock\n", R"({
          "schemaVersion":1,"status":"Passed","offline":true,
          "graphHash":"sha256:candidate","generationId":"candidate",
          "modules":[{"packageId":"pkg","version":"1","module":"instrumentation",
          "enabled":true,"runtimeReadiness":"Ready","artifactKinds":["apiSurface"],
          "apiSurfaces":["pkg/instrumentation"],"blocksSurfaces":[],
          "capabilities":["Diagnostics"],"license":"MPL-2.0",
          "trust":"TrustedEngineDistribution","origin":"fixture","offline":true}]})",
          "sha256:candidate"};
    }

    PackageUsageAnalysis AnalyzeUsage(const PackageWorkflowCapture& capture,
        std::string_view, std::string_view, const fs::path& stagingRoot,
        const PackageToolOperationContext&) const override
    {
        usageStagingRoot = stagingRoot;
        PackageUsageAnalysis result;
        result.passed = true;
        result.sourceInventoryHash = capture.sourceInventoryHash;
        result.generationId = capture.generationId;
        result.generationHash = capture.generationHash;
        result.graphHash = capture.graphHash;
        result.ownershipCatalogHash = capture.ownershipCatalogHash;
        result.projectHash = capture.projectHash;
        result.lockHash = capture.lockHash;
        result.generationManifestHash = capture.generationManifestHash;
        result.referenceInventoryHash = capture.referenceInventoryHash;
        if (reportUsage)
        {
            PackageUsageLocation location;
            location.sourcePath = "Scripts/Use.cs";
            location.startLine = 7;
            location.startColumn = 21;
            location.documentationId = "M:Fixture.Use";
            location.packageId = "pkg";
            location.module = "instrumentation";
            result.locations.push_back(std::move(location));
        }
        return result;
    }

    PackageInspectionResult RefreshGeneration(const PackageWorkflowCapture&,
        const PackageToolOperationContext&) const override
    {
        ++refreshCalls;
        return refreshResult;
    }
    PackageInspectionResult InspectCanonical(const PackageWorkflowCapture&,
        const PackageToolOperationContext&) const override { return {}; }

    mutable int resolveCalls = 0;
    mutable int refreshCalls = 0;
    mutable fs::path usageStagingRoot;
    bool reportUsage = false;
    PackageInspectionResult refreshResult;
private:
    fs::path root_;
};

class PackageWorkflowControllerTests : public testing::Test
{
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() / ("saga-package-controller-" +
            std::to_string(std::chrono::steady_clock::now()
                .time_since_epoch().count()));
        fs::create_directories(root);
        std::ofstream(root / "Fixture.sagaproj") <<
            R"({"dependencies":[{"packageId":"pkg","version":"1","modules":["core","instrumentation"]}]})";
        std::ofstream(root / "SagaPackage.lock") << "base-lock\n";
        client = std::make_shared<ProjectClient>(root);
        workspace = std::make_unique<EditorDocumentWorkspace>(client);
        ASSERT_TRUE(workspace->OpenProject(root));
        adapter = new Adapter(root);
        workflow = std::make_unique<PackageModuleWorkflow>(readiness, catalog);
        controller = std::make_unique<PackageWorkflowController>(*workspace,
            readiness, catalog, *workflow, analysis, projection, diagnostics,
            std::unique_ptr<IPackageWorkflowToolAdapter>(adapter));
    }

    void TearDown() override
    {
        controller.reset();
        workflow.reset();
        workspace.reset();
        fs::remove_all(root);
    }

    fs::path root;
    std::shared_ptr<ProjectClient> client;
    std::unique_ptr<EditorDocumentWorkspace> workspace;
    ProjectReadinessSession readiness;
    PackageStateCatalog catalog;
    std::unique_ptr<PackageModuleWorkflow> workflow;
    ScriptAnalysisCoordinator analysis;
    ScriptProjectionSession projection;
    EditorDiagnosticsService diagnostics;
    Adapter* adapter = nullptr;
    std::unique_ptr<PackageWorkflowController> controller;
};
}

TEST_F(PackageWorkflowControllerTests, SelectionPrepareAndCancelAreSerialized)
{
    controller->Select({"pkg", "instrumentation"});
    const auto prepared = controller->PrepareSelected(
        PackageModuleOperation::Disable);
    ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared);
    EXPECT_EQ(adapter->resolveCalls, 1);
    auto snapshot = controller->Snapshot();
    EXPECT_EQ(snapshot.state, PackageWorkflowControllerState::AwaitingReview);
    EXPECT_TRUE(snapshot.canApply);
    EXPECT_TRUE(snapshot.canCancel);
    EXPECT_EQ(controller->PrepareSelected(PackageModuleOperation::Disable).code,
        "editor.packages.reentrant");
    EXPECT_EQ(controller->Cancel().status, PackageWorkflowStatus::Cancelled);
    EXPECT_EQ(controller->Snapshot().state,
        PackageWorkflowControllerState::Ready);
    EXPECT_FALSE(workflow->Active());
    EXPECT_TRUE(adapter->usageStagingRoot.empty() ||
        adapter->usageStagingRoot.parent_path().filename() == "package");
}

TEST_F(PackageWorkflowControllerTests,
       FailedDerivedRefreshCanRetryWithoutChangingCommittedInputs)
{
    adapter->refreshResult = {false,
        "editor.packages.generation-refresh-failed", "fixture failure", {}};
    controller->Select({"pkg", "instrumentation"});
    const auto prepared = controller->PrepareSelected(
        PackageModuleOperation::Disable);
    ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared);
    ASSERT_FALSE(adapter->usageStagingRoot.empty());
    EXPECT_EQ(adapter->usageStagingRoot.parent_path().filename(), "package");

    const auto applied = controller->Apply();
    EXPECT_EQ(applied.status, PackageWorkflowStatus::ToolingOutOfDate);
    EXPECT_EQ(controller->Snapshot().state,
        PackageWorkflowControllerState::ToolingOutOfDate);
    EXPECT_TRUE(controller->Snapshot().canRetry);
    const auto manifest = Read(root / "Fixture.sagaproj");
    const auto lock = Read(root / "SagaPackage.lock");

    const auto retried = controller->RetryRefresh();
    EXPECT_EQ(retried.status, PackageWorkflowStatus::ToolingOutOfDate);
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), manifest);
    EXPECT_EQ(Read(root / "SagaPackage.lock"), lock);
    EXPECT_EQ(adapter->refreshCalls, 2);
    EXPECT_TRUE(ProjectMutationRecovery::Scan(root).empty());
}

TEST_F(PackageWorkflowControllerTests, UsageDiagnosticsPublishExactLocationWithoutSourceContents)
{
    adapter->reportUsage = true;
    controller->Select({"pkg", "instrumentation"});
    const auto result = controller->PrepareSelected(
        PackageModuleOperation::Disable);
    EXPECT_EQ(result.code, "Project.PackageTransaction.ModuleInUse");
    ASSERT_EQ(diagnostics.GetAll().size(), 1U);
    const auto& diagnostic = diagnostics.GetAll().front();
    EXPECT_EQ(diagnostic.source, "editor.packages");
    EXPECT_EQ(diagnostic.location.resource, "Scripts/Use.cs");
    EXPECT_EQ(diagnostic.location.line, 7);
    EXPECT_EQ(diagnostic.location.column, 21);
    EXPECT_EQ(diagnostic.message.find("Fixture.Use"), std::string::npos);
}
