// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Panels/HierarchyPanel.h"
#include "SagaEditor/Panels/InspectorPanel.h"
#include "SagaEditor/Panels/CodeBoxPanel.h"
#include "SagaEditor/Panels/PackagesPanel.h"
#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/SceneEditing/SceneSelectionIdentity.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTreeWidget>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

namespace
{
namespace fs = std::filesystem;

QApplication& Application()
{
    static const bool configured = [] { qputenv("QT_QPA_PLATFORM", "offscreen"); return true; }();
    (void)configured;
    if (auto* existing = qobject_cast<QApplication*>(QCoreApplication::instance()))
        return *existing;
    static int argc = 1;
    static char name[] = "SagaEditorQtTests";
    static char* argv[] = {name, nullptr};
    static QApplication application(argc, argv);
    return application;
}

class FakeClient final : public SagaEditor::IProjectToolClient
{
public:
    SagaEditor::ProjectToolResult result;
    SagaEditor::ProjectToolResult Validate(const fs::path&) const override { return result; }
    SagaEditor::ProjectToolResult CreateStarterArena(
        const SagaEditor::ProjectTemplateRequest&) const override { return result; }
};

void Write(const fs::path& path, std::string_view bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}

struct Fixture
{
    fs::path root;
    std::shared_ptr<FakeClient> client = std::make_shared<FakeClient>();
    SagaEditor::EditorDocumentWorkspace workspace{client};

    Fixture()
    {
        root = fs::temp_directory_path() / ("saga-qt-authoring-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        Write(root / "Project.sagaproj", "{}\n");
        Write(root / "Scene.sagascene",
            R"({"schemaVersion":1,"sceneId":"main","entities":[{"entityId":"root","name":"Root","components":[{"type":"Transform","position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}]},{"entityId":"door","parentEntityId":"root","name":"Door","components":[{"type":"Transform","position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}]}]})");
        Write(root / "Scripts/Door.cs", "public class Door {}\n");
        client->result.passed = true;
        client->result.project.projectId = "f2ea2b57-ed20-4daa-87ee-acaf79681b5b";
        client->result.project.displayName = "Qt Fixture";
        client->result.project.projectRoot = root;
        client->result.project.manifestPath = root / "Project.sagaproj";
        client->result.project.sources = {
            {"scene", "main", "scene", "Scene.sagascene", root / "Scene.sagascene"},
            {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"}};
        EXPECT_TRUE(workspace.OpenProject(root));
    }
    ~Fixture() { fs::remove_all(root); }
};
}

TEST(QtAuthoringPanelsTests, HierarchySelectionDrivesCanonicalInspector)
{
    (void)Application();
    Fixture fixture;
    SagaEditor::SelectionManager selection;
    SagaEditor::UndoRedoStack journal;
    SagaEditor::HierarchyPanel hierarchy;
    hierarchy.SetDocumentWorkspace(&fixture.workspace);
    hierarchy.SetSelectionManager(&selection);
    hierarchy.OnInit();
    SagaEditor::InspectorPanel inspector;
    inspector.SetDocumentWorkspace(&fixture.workspace);
    inspector.SetSelectionManager(&selection);
    inspector.SetUndoRedoStack(&journal);
    inspector.OnInit();

    auto* tree = hierarchy.Widget()->findChild<QTreeWidget*>();
    ASSERT_NE(tree, nullptr);
    const auto doorItems = tree->findItems("Door", Qt::MatchExactly | Qt::MatchRecursive);
    ASSERT_EQ(doorItems.size(), 1);
    doorItems.front()->setSelected(true);
    QApplication::processEvents();
    EXPECT_EQ(selection.GetPrimary(), SagaEditor::SceneSelectionId("main", "door"));
    auto* inspectorWidget = inspector.Widget();
    EXPECT_FALSE(inspectorWidget->findChildren<QLineEdit*>().empty());
    hierarchy.OnShutdown(); inspector.OnShutdown();
}

TEST(QtAuthoringPanelsTests, RestoredFloatingDockIsClampedToReachableScreen)
{
    (void)Application();
    QMainWindow owner;
    auto* stack = new QStackedWidget(&owner);
    owner.setCentralWidget(stack);
    SagaEditor::QtUIMainWindow window(owner, *stack);
    window.DockPanel(new QWidget(), "test.floating", "Floating",
        SagaEditor::UIDockArea::Right);
    owner.show();
    QApplication::processEvents();
    auto* dock = owner.findChild<QDockWidget*>("test.floating");
    ASSERT_NE(dock, nullptr);
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    dock->setFloating(true);
    dock->setGeometry(available.right() + 500, available.bottom() + 500,
        320, 240);
    const auto state = window.SaveState();
    dock->setGeometry(available.topLeft().x(), available.topLeft().y(), 320, 240);
    ASSERT_TRUE(window.RestoreState(state));
    EXPECT_TRUE(available.intersects(dock->frameGeometry()));
}

TEST(QtAuthoringPanelsTests, CodeBoxMutationUsesTransactionAndDoesNotWriteDisk)
{
    (void)Application();
    Fixture fixture;
    SagaEditor::UndoRedoStack journal;
    SagaEditor::CommandRegistry registry;
    SagaEditor::CommandDispatcher dispatcher(registry);
    SagaEditor::CodeBoxPanel panel;
    panel.SetDocumentWorkspace(&fixture.workspace);
    panel.SetUndoRedoStack(&journal);
    panel.SetCommandDispatcher(&dispatcher);
    panel.OnInit();

    auto* tabs = panel.Widget()->findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(tabs->count(), 2);
    EXPECT_EQ(tabs->tabText(0), "C#");
    EXPECT_EQ(tabs->tabText(1), QStringLiteral("Blocks - Controlled C# authoring"));
    auto* blocks = panel.Widget()->findChild<QTreeWidget*>();
    ASSERT_NE(blocks, nullptr);
    EXPECT_EQ(blocks->accessibleName(), "Controlled script blocks");

    auto* editor = panel.Widget()->findChild<QPlainTextEdit*>();
    ASSERT_NE(editor, nullptr);
    editor->setPlainText("public class Door { public void Open() {} }\n");
    QApplication::processEvents();
    EXPECT_TRUE(fixture.workspace.Scripts().front()->Snapshot().dirty);
    EXPECT_TRUE(journal.CanUndo());
    std::ifstream disk(fixture.root / "Scripts/Door.cs", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(disk),
                          std::istreambuf_iterator<char>()), "public class Door {}\n");
    panel.OnShutdown();
}

TEST(QtAuthoringPanelsTests, CodeBoxSelectsEachCanonicalSourceDocument)
{
    (void)Application();
    Fixture fixture;
    Write(fixture.root / "Scripts/Second.cs", "public class Second {}\n");
    fixture.workspace.Close();
    ASSERT_TRUE(fixture.workspace.OpenProject(fixture.root / "Project.sagaproj"));
    ASSERT_EQ(fixture.workspace.Scripts().size(), 2u);

    SagaEditor::UndoRedoStack journal;
    SagaEditor::CodeBoxPanel panel;
    panel.SetUndoRedoStack(&journal);
    panel.SetDocumentWorkspace(&fixture.workspace);
    panel.OnInit();
    auto* selector = panel.Widget()->findChild<QComboBox*>();
    auto* editor = panel.Widget()->findChild<QPlainTextEdit*>();
    ASSERT_NE(selector, nullptr);
    ASSERT_NE(editor, nullptr);
    ASSERT_EQ(selector->count(), 2);
    selector->setCurrentIndex(1);
    QApplication::processEvents();
    EXPECT_EQ(editor->toPlainText(), QStringLiteral("public class Second {}\n"));
    panel.OnShutdown();
}

TEST(QtAuthoringPanelsTests, PackagesPanelRendersVerifiedPermanentCatalogState)
{
    (void)Application();
    SagaEditor::PackageStateCatalog catalog;
    std::string diagnostic;
    ASSERT_TRUE(catalog.LoadInspection(R"({
      "schemaVersion":1,"status":"Passed","offline":true,
      "graphHash":"sha256:graph","generationId":"generation",
      "modules":[
        {"packageId":"challenge","version":"1","module":"core","enabled":true,
         "runtimeReadiness":"Ready","artifactKinds":["apiSurface"],
         "apiSurfaces":["challenge/core"],"blocksSurfaces":["challenge/core"],
         "capabilities":["Gameplay"],"license":"MPL-2.0",
         "trust":"TrustedEngineDistribution","origin":"built-in","offline":true},
        {"packageId":"challenge","version":"1","module":"instrumentation","enabled":false,
         "runtimeReadiness":"Ready","artifactKinds":["apiSurface"],
         "apiSurfaces":["challenge/instrumentation"],"blocksSurfaces":[],
         "capabilities":["DevelopmentDiagnostics"],"license":"MPL-2.0",
         "trust":"TrustedEngineDistribution","origin":"built-in","offline":true}
      ]})", diagnostic)) << diagnostic;
    SagaEditor::ProjectReadinessSession readiness;
    SagaEditor::PackageModuleWorkflow workflow(readiness, catalog);
    SagaEditor::PackagesPanel panel;
    panel.SetCatalog(&catalog);
    panel.SetReadiness(&readiness);
    panel.SetWorkflow(&workflow);
    panel.OnInit();
    EXPECT_EQ(panel.GetPanelId(), "saga.panel.packages");
    auto* tree = panel.Widget()->findChild<QTreeWidget*>();
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->topLevelItemCount(), 2);
    EXPECT_EQ(tree->topLevelItem(0)->text(3), "Enabled");
    EXPECT_EQ(tree->topLevelItem(1)->text(2), "instrumentation");
    EXPECT_EQ(tree->topLevelItem(1)->text(3), "Available");
    tree->topLevelItem(1)->setSelected(true);
    QApplication::processEvents();
    const auto buttons = panel.Widget()->findChildren<QPushButton*>();
    const auto enable = std::find_if(buttons.begin(), buttons.end(),
        [](auto* button) { return button->text() == "Enable module…"; });
    ASSERT_NE(enable, buttons.end());
    EXPECT_TRUE((*enable)->isEnabled());
    readiness.SetMutating(true);
    panel.Refresh();
    EXPECT_FALSE((*enable)->isEnabled());
    panel.OnShutdown();
}

TEST(QtAuthoringPanelsTests, RecoveryButtonsForwardExplicitDocumentGestures)
{
    (void)Application();
    Fixture fixture;
    ASSERT_TRUE(fixture.workspace.Scripts().front()->ReplaceText(
        "public class RecoveredDoor {}\n"));
    const auto journal = fixture.workspace.Scripts().front()->Snapshot().recoveryReference;
    ASSERT_TRUE(fs::is_regular_file(journal));
    fixture.workspace.Close();
    ASSERT_TRUE(fixture.workspace.OpenProject(fixture.root / "Project.sagaproj"));
    ASSERT_EQ(fixture.workspace.Scripts().front()->Recovery().status,
        SagaEditor::DocumentRecoveryStatus::Recoverable);

    auto findButton = [](QWidget* widget, const QString& label)
    {
        for (auto* button : widget->findChildren<QPushButton*>())
            if (button->text() == label) return button;
        return static_cast<QPushButton*>(nullptr);
    };
    {
        SagaEditor::CodeBoxPanel panel;
        panel.SetDocumentWorkspace(&fixture.workspace);
        panel.OnInit();
        auto* widget = panel.Widget();
        auto* recover = findButton(widget, QStringLiteral("Recover"));
        ASSERT_NE(recover, nullptr);
        EXPECT_FALSE(recover->isHidden());
        recover->click();
        QApplication::processEvents();
        EXPECT_TRUE(fixture.workspace.Scripts().front()->Snapshot().dirty);
        EXPECT_EQ(fixture.workspace.Scripts().front()->Text(),
            "public class RecoveredDoor {}\n");
        panel.OnShutdown();
    }

    fixture.workspace.Close();
    ASSERT_TRUE(fixture.workspace.OpenProject(fixture.root / "Project.sagaproj"));
    {
        SagaEditor::CodeBoxPanel panel;
        panel.SetDocumentWorkspace(&fixture.workspace);
        panel.OnInit();
        auto* widget = panel.Widget();
        auto* discard = findButton(widget, QStringLiteral("Discard recovery"));
        ASSERT_NE(discard, nullptr);
        EXPECT_FALSE(discard->isHidden());
        discard->click();
        QApplication::processEvents();
        EXPECT_FALSE(fs::exists(journal));
        EXPECT_EQ(fixture.workspace.Scripts().front()->Recovery().status,
            SagaEditor::DocumentRecoveryStatus::None);
        panel.OnShutdown();
    }
}
