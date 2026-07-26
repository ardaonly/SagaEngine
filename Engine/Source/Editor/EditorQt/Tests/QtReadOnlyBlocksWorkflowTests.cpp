// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Panels/ScriptingPanel.h"
#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/Panels/ReadOnlyBlocksWidget.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

namespace
{
namespace fs = std::filesystem;
namespace Blocks = SagaEditor::VisualBlocks;

QApplication& Application()
{
    static const bool configured = [] { qputenv("QT_QPA_PLATFORM", "offscreen"); return true; }();
    (void)configured;
    if (auto* existing = qobject_cast<QApplication*>(QCoreApplication::instance()))
        return *existing;
    static int argc = 1;
    static char name[] = "SagaEditorQtBlocksTests";
    static char* argv[] = {name, nullptr};
    static QApplication application(argc, argv);
    return application;
}

Blocks::ProjectionSourceSpan Span(std::uint64_t start, std::uint64_t end)
{
    return {"Scripts/Door.cs", "sha256:source", start, end, 1, 1, 1, 1};
}

std::shared_ptr<const Blocks::ReadOnlyScriptProjection> Projection(
    Blocks::ProjectionSourceSpan eventSpan = Span(0, 4),
    Blocks::ProjectionSourceSpan actionSpan = Span(5, 11),
    Blocks::ProjectionSourceSpan opaqueSpan = Span(12, 18))
{
    std::vector<Blocks::ProjectionCard> children;
    children.emplace_back("toggle", "statement", "toggle door target state",
        "StarterArena.Scripts", actionSpan);
    std::vector<Blocks::ProjectionCard> roots;
    roots.emplace_back("interact", "event", "WHEN Interact",
        "StarterArena.Scripts", eventSpan, std::move(children));
    std::vector<Blocks::ProjectionCard> opaque;
    opaque.emplace_back("opaque", "opaque", "Opaque C# region",
        "StarterArena.Scripts", opaqueSpan);
    return std::make_shared<const Blocks::ReadOnlyScriptProjection>(
        "project", "sha256:project", "sha256:source", "packages-17",
        "sha256:packages", "sha256:projection", std::move(roots), std::move(opaque));
}

SagaEditor::ScriptProjectionRequestIdentity Identity()
{
    return {7, "sha256:source", 11, "sha256:project", "packages-17",
        "sha256:packages", "analysis-31"};
}

SagaEditor::ScriptProjectionSnapshot Snapshot(
    SagaEditor::ScriptProjectionState state, std::string diagnostic = {},
    std::shared_ptr<const Blocks::ReadOnlyScriptProjection> projection = {})
{
    SagaEditor::ScriptProjectionSnapshot snapshot;
    snapshot.state = state;
    snapshot.diagnostic = std::move(diagnostic);
    snapshot.projection = std::move(projection);
    return snapshot;
}

QLabel* Status(SagaEditor::ReadOnlyBlocksWidget& widget)
{
    for (auto* label : widget.findChildren<QLabel*>())
        if (label->accessibleName() == QStringLiteral("Blocks projection status")) return label;
    return nullptr;
}

class FakeClient final : public SagaEditor::IProjectToolClient
{
public:
    SagaEditor::ProjectToolResult result;
    SagaEditor::ProjectToolResult Validate(const fs::path&) const override { return result; }
    SagaEditor::ProjectToolResult CreateStarterArena(
        const SagaEditor::ProjectTemplateRequest&) const override { return result; }
};

class FailOnCallMutationPort final : public SagaEditor::IScriptSourceMutationPort
{
public:
    [[nodiscard]] bool ReplaceText(SagaEditor::CSharpSourceDocument&,
        SagaEditor::UndoRedoStack&, std::string) override
    {
        ++calls;
        ADD_FAILURE() << "Blocks navigation attempted source mutation";
        return false;
    }

    std::size_t calls = 0;
};

std::string ReadBinary(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

class WorkspaceFixture final
{
public:
    WorkspaceFixture()
        : root(fs::temp_directory_path() / ("saga-qt-blocks-" + std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()))),
          client(std::make_shared<FakeClient>()), workspace(client)
    {
        const auto write = [](const fs::path& path, std::string_view bytes)
        {
            fs::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary | std::ios::trunc) << bytes;
        };
        write(root / "Project.sagaproj", "{}\n");
        write(root / "Scene.sagascene", R"({"schemaVersion":1,"sceneId":"main","entities":[]})");
        write(root / "Scripts/Door.cs", source);
        client->result.passed = true;
        client->result.project.projectId = "f2ea2b57-ed20-4daa-87ee-acaf79681b5b";
        client->result.project.projectRoot = root;
        client->result.project.manifestPath = root / "Project.sagaproj";
        client->result.project.sources = {
            {"scene", "main", "scene", "Scene.sagascene", root / "Scene.sagascene"},
            {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"}};
        EXPECT_TRUE(workspace.OpenProject(root));
    }

    ~WorkspaceFixture() { fs::remove_all(root); }

    static constexpr std::string_view source =
        "public class Door { void Interact() { Start(); } }\n";
    fs::path root;
    std::shared_ptr<FakeClient> client;
    SagaEditor::EditorDocumentWorkspace workspace;
};
}

TEST(QtReadOnlyBlocksWorkflowTests, PresentsRealNestedProjectionAndOpaqueRegion)
{
    (void)Application();
    SagaEditor::ScriptProjectionSession session;
    SagaEditor::ReadOnlyBlocksWidget widget;
    session.Begin(Identity());
    widget.PresentSnapshot(session.Snapshot());
    ASSERT_NE(Status(widget), nullptr);
    EXPECT_EQ(Status(widget)->text(), QStringLiteral("Analyzing…"));
    ASSERT_TRUE(session.Publish(Identity(), "sha256:projection", Projection()));
    widget.PresentSnapshot(session.Snapshot());

    auto* tree = widget.findChild<QTreeWidget*>();
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->topLevelItemCount(), 2);
    auto* when = tree->topLevelItem(0);
    EXPECT_EQ(when->text(0), QStringLiteral("WHEN Interact"));
    EXPECT_EQ(when->text(1), QStringLiteral("StarterArena.Scripts"));
    ASSERT_EQ(when->childCount(), 1);
    EXPECT_EQ(when->child(0)->text(0), QStringLiteral("toggle door target state"));
    EXPECT_EQ(tree->topLevelItem(1)->text(0), QStringLiteral("Opaque C# region"));
    EXPECT_FALSE(when->flags().testFlag(Qt::ItemIsEditable));
    EXPECT_TRUE(Status(widget)->text().contains(QStringLiteral("Current")));
    EXPECT_TRUE(Status(widget)->text().contains(QStringLiteral("packages-17")));
}

TEST(QtReadOnlyBlocksWorkflowTests, PresentsLifecycleStatesAndClearsOldCards)
{
    (void)Application();
    SagaEditor::ReadOnlyBlocksWidget widget;
    const std::vector<std::pair<SagaEditor::ScriptProjectionSnapshot, QString>> states = {
        {Snapshot(SagaEditor::ScriptProjectionState::DirtyNeedsSave), "Save changes before analysis"},
        {Snapshot(SagaEditor::ScriptProjectionState::Stale), "Stale projection rejected"},
        {Snapshot(SagaEditor::ScriptProjectionState::Failed, "VB100: invalid source"),
            "Projection failed: VB100: invalid source"},
        {Snapshot(SagaEditor::ScriptProjectionState::ProjectClosed), "Project closed"},
        {Snapshot(SagaEditor::ScriptProjectionState::PackageGenerationChanged),
            "Package generation changed"}};

    widget.Present(Projection());
    for (const auto& [snapshot, expected] : states)
    {
        widget.PresentSnapshot(snapshot);
        EXPECT_EQ(Status(widget)->text(), expected);
        EXPECT_EQ(widget.findChild<QTreeWidget*>()->topLevelItemCount(), 0);
    }
}

TEST(QtReadOnlyBlocksWorkflowTests, ActivatesExactNestedAndOpaqueSourceSpans)
{
    (void)Application();
    SagaEditor::ReadOnlyBlocksWidget widget;
    widget.Present(Projection());
    std::vector<Blocks::ProjectionSourceSpan> activated;
    widget.SetSourceActivated([&activated](const auto& span) { activated.push_back(span); });
    auto* tree = widget.findChild<QTreeWidget*>();
    Q_EMIT tree->itemActivated(tree->topLevelItem(0)->child(0), 0);
    Q_EMIT tree->itemActivated(tree->topLevelItem(1), 0);
    ASSERT_EQ(activated.size(), 2U);
    EXPECT_EQ(activated[0], Span(5, 11));
    EXPECT_EQ(activated[1], Span(12, 18));
}

TEST(QtReadOnlyBlocksWorkflowTests, PanelNavigationDoesNotMutateOrDirtySource)
{
    (void)Application();
    WorkspaceFixture fixture;
    SagaEditor::UndoRedoStack journal;
    FailOnCallMutationPort mutationSpy;
    SagaEditor::ScriptingPanel panel;
    panel.SetDocumentWorkspace(&fixture.workspace);
    panel.SetUndoRedoStack(&journal);
    panel.SetScriptSourceMutationPort(&mutationSpy);
    panel.OnInit();
    auto* root = panel.Widget();
    SagaEditor::ReadOnlyBlocksWidget* blocks = nullptr;
    for (auto* child : root->findChildren<QWidget*>())
        if ((blocks = dynamic_cast<SagaEditor::ReadOnlyBlocksWidget*>(child))) break;
    ASSERT_NE(blocks, nullptr);
    const auto text = fixture.workspace.Scripts().front()->Text();
    const auto start = text.find("Interact");
    const auto end = start + std::string_view("Interact").size();
    blocks->Present(Projection(Span(start, end), Span(start, end), Span(0, 6)));

    const auto before = fixture.workspace.Scripts().front()->Snapshot();
    const auto diskBefore = ReadBinary(fixture.root / "Scripts/Door.cs");
    auto* tree = blocks->findChild<QTreeWidget*>();
    Q_EMIT tree->itemActivated(tree->topLevelItem(0)->child(0), 0);
    QApplication::processEvents();
    auto* tabs = root->findChild<QTabWidget*>();
    auto* editor = root->findChild<QPlainTextEdit*>();
    ASSERT_NE(tabs, nullptr);
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(tabs->tabText(tabs->currentIndex()), QStringLiteral("Scripting"));
    EXPECT_EQ(editor->textCursor().selectedText(), QStringLiteral("Interact"));
    Q_EMIT tree->itemActivated(tree->topLevelItem(1), 0);
    QApplication::processEvents();
    EXPECT_EQ(editor->textCursor().selectedText(), QStringLiteral("public"));
    EXPECT_EQ(mutationSpy.calls, 0U);
    EXPECT_EQ(fixture.workspace.Scripts().front()->Text(), text);
    EXPECT_EQ(fixture.workspace.Scripts().front()->Snapshot().inMemoryRevision,
        before.inMemoryRevision);
    EXPECT_FALSE(fixture.workspace.Scripts().front()->Snapshot().dirty);
    EXPECT_FALSE(journal.CanUndo());
    EXPECT_EQ(ReadBinary(fixture.root / "Scripts/Door.cs"), diskBefore);
    panel.OnShutdown();
}

TEST(QtReadOnlyBlocksWorkflowTests, QueuedPresentationCannotReachDestroyedWidget)
{
    (void)Application();
    auto* widget = new SagaEditor::ReadOnlyBlocksWidget;
    QPointer<SagaEditor::ReadOnlyBlocksWidget> guard(widget);
    QTimer::singleShot(0, [guard]
    {
        if (guard) guard->PresentSnapshot(
            Snapshot(SagaEditor::ScriptProjectionState::Current, {}, Projection()));
    });
    delete widget;
    QApplication::processEvents();
    EXPECT_TRUE(guard.isNull());
}
