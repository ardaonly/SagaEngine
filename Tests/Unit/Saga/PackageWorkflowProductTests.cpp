// SPDX-License-Identifier: MPL-2.0

#include "SagaProductTestSupport.hpp"

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/Packages/PackageWorkflowController.h"
#include "SagaEditor/Project/ProjectMutation.h"
#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <cstdlib>

using namespace SagaProductTestSupport;

namespace
{
using namespace SagaEditor;

class ProductPackageWorkflowFixture : public testing::Test
{
protected:
    void SetUp() override
    {
        root = MakeTempDir("saga_product_package_workflow") / "ArenaRush";
        fs::copy(SourceRoot() / "Samples/ArenaRush", root,
            fs::copy_options::recursive);
        ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));
        ASSERT_TRUE(host.GetDocumentWorkspace().OpenProject(
            root / "ArenaRush.sagaproj"));
    }

    void TearDown() override
    {
        host.Shutdown();
        fs::remove_all(root.parent_path());
    }

    void ExpectReady(bool enabled)
    {
        const auto snapshot = host.GetPackageWorkflowController().Snapshot();
        EXPECT_EQ(snapshot.state, PackageWorkflowControllerState::Ready);
        EXPECT_TRUE(host.GetProjectReadinessSession().CanPlayOrBuild());
        const auto* module = host.GetPackageStateCatalog().Find(
            "com.saga.gameplay.challenge", "collectibles.instrumentation");
        ASSERT_NE(module, nullptr);
        EXPECT_EQ(module->enabled, enabled);
        const auto& projection = host.GetScriptProjectionSession().Snapshot();
        EXPECT_EQ(projection.state, ScriptProjectionState::Current);
        ASSERT_NE(projection.projection, nullptr);
        EXPECT_EQ(projection.identity.packageGenerationId,
            host.GetPackageStateCatalog().GenerationId());
        EXPECT_EQ(projection.identity.projectHash,
            projection.projection->ProjectHash());
        {
            auto [lease, acquired] = ProjectMutationLease::Acquire(root,
                "product-test-probe");
            EXPECT_TRUE(acquired.succeeded)
                << acquired.code << ": " << acquired.message;
            EXPECT_TRUE(lease.Held());
        }
        EXPECT_TRUE(ProjectMutationRecovery::Scan(root).empty());
    }

    void Apply(PackageModuleOperation operation)
    {
        auto& controller = host.GetPackageWorkflowController();
        controller.Select({"com.saga.gameplay.challenge",
            "collectibles.instrumentation"});
        const auto prepared = controller.PrepareSelected(operation);
        ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared)
            << prepared.code << ": " << prepared.message;
        ASSERT_TRUE(prepared.review.has_value());
        const auto applied = controller.Apply();
        ASSERT_EQ(applied.status, PackageWorkflowStatus::Applied)
            << applied.code << ": " << applied.message;
    }

    fs::path root;
    EditorHost host;
};

class ProcessEnvironmentGuard final
{
public:
    ProcessEnvironmentGuard()
        : workingDirectory_(fs::current_path())
    {
        if (const auto* value = std::getenv("PATH")) path_ = value;
    }
    ~ProcessEnvironmentGuard()
    {
        fs::current_path(workingDirectory_);
        if (path_) setenv("PATH", path_->c_str(), 1);
        else unsetenv("PATH");
    }
private:
    fs::path workingDirectory_;
    std::optional<std::string> path_;
};
}

TEST_F(ProductPackageWorkflowFixture,
       CanonicalOpenPrepareCancelAndTwentyEnableDisableCycles)
{
    auto& controller = host.GetPackageWorkflowController();
    const auto opened = controller.OpenOrRefresh();
    ASSERT_EQ(opened.status, PackageWorkflowStatus::Applied)
        << opened.code << ": " << opened.message;
    ASSERT_NO_FATAL_FAILURE(ExpectReady(false));

    controller.Select({"com.saga.gameplay.challenge",
        "collectibles.instrumentation"});
    const auto prepared = controller.PrepareSelected(
        PackageModuleOperation::Enable);
    ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared)
        << prepared.code << ": " << prepared.message;
    EXPECT_EQ(controller.Cancel().status, PackageWorkflowStatus::Cancelled);
    ASSERT_NO_FATAL_FAILURE(ExpectReady(false));

    for (int cycle = 0; cycle < 20; ++cycle)
    {
        ASSERT_NO_FATAL_FAILURE(Apply(PackageModuleOperation::Enable));
        ASSERT_NO_FATAL_FAILURE(ExpectReady(true));
        ASSERT_NO_FATAL_FAILURE(Apply(PackageModuleOperation::Disable));
        ASSERT_NO_FATAL_FAILURE(ExpectReady(false));
    }

    host.GetDocumentWorkspace().Close();
    ASSERT_TRUE(host.GetDocumentWorkspace().OpenProject(
        root / "ArenaRush.sagaproj"));
    const auto reopened = controller.OpenOrRefresh();
    ASSERT_EQ(reopened.status, PackageWorkflowStatus::Applied)
        << reopened.code << ": " << reopened.message;
    ASSERT_NO_FATAL_FAILURE(ExpectReady(false));
}

TEST_F(ProductPackageWorkflowFixture,
       FreshUndoRedoModuleUsageAndStaleSourceUseConcreteProductPath)
{
    auto& controller = host.GetPackageWorkflowController();
    ASSERT_EQ(controller.OpenOrRefresh().status, PackageWorkflowStatus::Applied);
    ASSERT_NO_FATAL_FAILURE(Apply(PackageModuleOperation::Enable));
    ASSERT_NO_FATAL_FAILURE(ExpectReady(true));

    const auto undo = controller.PrepareUndo();
    ASSERT_EQ(undo.status, PackageWorkflowStatus::Prepared)
        << undo.code << ": " << undo.message;
    ASSERT_EQ(controller.Apply().status, PackageWorkflowStatus::Applied);
    ASSERT_NO_FATAL_FAILURE(ExpectReady(false));
    const auto redo = controller.PrepareRedo();
    ASSERT_EQ(redo.status, PackageWorkflowStatus::Prepared)
        << redo.code << ": " << redo.message;
    ASSERT_EQ(controller.Apply().status, PackageWorkflowStatus::Applied);
    ASSERT_NO_FATAL_FAILURE(ExpectReady(true));

    {
        std::ofstream source(root / "Scripts/InstrumentationUse.cs");
        source << "public static class InstrumentationUse { public static void Run() { "
            "_ = SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics."
            "CollectibleInstrumentation.Create(1); } }\n";
    }
    host.GetDocumentWorkspace().Close();
    ASSERT_TRUE(host.GetDocumentWorkspace().OpenProject(
        root / "ArenaRush.sagaproj"));
    ASSERT_EQ(controller.OpenOrRefresh().status, PackageWorkflowStatus::Applied);
    controller.Select({"com.saga.gameplay.challenge",
        "collectibles.instrumentation"});
    const auto inUse = controller.PrepareSelected(PackageModuleOperation::Disable);
    EXPECT_EQ(inUse.code, "Project.PackageTransaction.ModuleInUse");
    ASSERT_FALSE(host.GetEditorDiagnosticsService().GetAll().empty());
    EXPECT_EQ(host.GetEditorDiagnosticsService().GetAll().front().source,
        "editor.packages");

    fs::remove(root / "Scripts/InstrumentationUse.cs");
    host.GetDocumentWorkspace().Close();
    ASSERT_TRUE(host.GetDocumentWorkspace().OpenProject(
        root / "ArenaRush.sagaproj"));
    ASSERT_EQ(controller.OpenOrRefresh().status, PackageWorkflowStatus::Applied);
    controller.Select({"com.saga.gameplay.challenge",
        "collectibles.instrumentation"});
    const auto prepared = controller.PrepareSelected(PackageModuleOperation::Disable);
    ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared)
        << prepared.code << ": " << prepared.message;
    std::ofstream(root / "Scripts/ArenaRushGameMode.cs", std::ios::app) << "\n";
    const auto stale = controller.Apply();
    EXPECT_EQ(stale.code, "Project.PackageTransaction.StaleUsageAnalysis");
    EXPECT_TRUE(ProjectMutationRecovery::Scan(root).empty());
}

TEST_F(ProductPackageWorkflowFixture,
       ManagedToolsIgnoreHostilePathAndUnrelatedWorkingDirectory)
{
    ProcessEnvironmentGuard environment;
    const auto unrelated = root.parent_path() / "unrelated-working-directory";
    fs::create_directories(unrelated);
    fs::current_path(unrelated);
    setenv("PATH", "/hostile/path/that/contains/no/tools", 1);

    auto& controller = host.GetPackageWorkflowController();
    const auto opened = controller.OpenOrRefresh();
    ASSERT_EQ(opened.status, PackageWorkflowStatus::Applied)
        << opened.code << ": " << opened.message;
    controller.Select({"com.saga.gameplay.challenge",
        "collectibles.instrumentation"});
    const auto prepared = controller.PrepareSelected(PackageModuleOperation::Enable);
    ASSERT_EQ(prepared.status, PackageWorkflowStatus::Prepared)
        << prepared.code << ": " << prepared.message;
    EXPECT_EQ(controller.Cancel().status, PackageWorkflowStatus::Cancelled);
}
