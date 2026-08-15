// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherLifecycleTests.cpp
/// @brief Saga Home and attached-editor lifecycle contracts.

#include "SagaLauncherUiTestSupport.h"

namespace
{
TEST(SagaHomeUiTest, PermanentHomeActionsAndModesExist)
{
    SagaProduct::SagaMainWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();

    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    EXPECT_NE(window.findChild<QWidget*>("SagaHomePage"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaHomeNewProject"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaHomeOpenProject"), nullptr);
    EXPECT_NE(window.findChild<QListWidget*>("SagaHomeRecentProjects"), nullptr);
    auto* modes = window.findChild<QComboBox*>("SagaHomeAuthoringMode");
    ASSERT_NE(modes, nullptr);
    EXPECT_EQ(modes->count(), 3);
    EXPECT_EQ(modes->itemData(0).toString(), "quick-production");
    EXPECT_EQ(modes->itemData(1).toString(), "balanced");
    EXPECT_EQ(modes->itemData(2).toString(), "full-control");
}

TEST(SagaHomeUiTest, RoleRouterAndReportControlsAreAbsent)
{
    SagaProduct::SagaMainWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();

    QStringList labels;
    for (const auto* button : window.findChildren<QPushButton*>())
        labels.push_back(button->text());
    const QString joined = labels.join('|');
    EXPECT_FALSE(joined.contains("Open Editor"));
    EXPECT_FALSE(joined.contains("Validate Project"));
    EXPECT_FALSE(joined.contains("Target"));
    EXPECT_FALSE(joined.contains("Report"));
    EXPECT_FALSE(joined.contains("EditorLab"));
}

TEST(SagaHomeUiTest, FailedOpenStaysHomeAndEditorCanUnmountAndRemount)
{
    SagaProduct::SagaMainWindow window(QApplication::applicationFilePath().toStdString());
    ASSERT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    EXPECT_FALSE(window.OpenProject("missing-project.sagaproj"));
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);

    const auto project = TestSourceRoot() /
        "Samples/StarterArena/StarterArena.sagaproj";
    ASSERT_TRUE(window.OpenProject(project));
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Editor);
    EXPECT_EQ(window.windowTitle(), "Saga — StarterArena");
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.hierarchy"), nullptr);
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.inspector"), nullptr);

    window.ReturnHome();
    QApplication::processEvents();
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    EXPECT_EQ(window.windowTitle(), "Saga");
    EXPECT_TRUE(window.findChildren<QDockWidget*>().empty());

    ASSERT_TRUE(window.OpenProject(project));
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Editor);
    window.ReturnHome();
    QApplication::processEvents();
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
}

TEST(SagaHomeUiTest, NewProjectUsesCanonicalWorkflowAndOpensEditor)
{
    QTemporaryDir parent;
    ASSERT_TRUE(parent.isValid());
    const auto destination =
        std::filesystem::path(parent.path().toStdString()) / "CreatedFromHome";
    ASSERT_FALSE(std::filesystem::exists(destination));
    SagaProduct::SagaMainWindow window(
        QApplication::applicationFilePath().toStdString());

    const auto result =
        window.CreateProject(destination, "Created From Home");

    ASSERT_TRUE(result.passed) <<
        (result.diagnostics.empty() ? std::string{} :
         result.diagnostics.front().code + ": " +
         result.diagnostics.front().message);
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Editor);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        result.project.manifestPath));
    EXPECT_EQ(result.project.manifestPath.parent_path(), destination);
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.hierarchy"), nullptr);
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.inspector"), nullptr);

    window.ReturnHome();
    QApplication::processEvents();
    ASSERT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    auto* recents = window.findChild<QListWidget*>("SagaHomeRecentProjects");
    ASSERT_NE(recents, nullptr);
    bool found = false;
    for (int index = 0; index < recents->count(); ++index)
        found = found || recents->item(index)->data(Qt::UserRole).toString() ==
            QString::fromStdString(result.project.manifestPath.string());
    EXPECT_TRUE(found);
}

TEST(SagaHomeUiTest, ExistingDestinationIsRejectedWithoutOverwrite)
{
    QTemporaryDir parent;
    ASSERT_TRUE(parent.isValid());
    const auto destination =
        std::filesystem::path(parent.path().toStdString()) / "Existing";
    std::filesystem::create_directories(destination);
    const auto sentinel = destination / "keep.txt";
    std::ofstream(sentinel) << "do not overwrite\n";
    SagaProduct::SagaMainWindow window(
        QApplication::applicationFilePath().toStdString());

    const auto result = window.CreateProject(destination, "Existing");

    ASSERT_FALSE(result.passed);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "Project.Template.CreateFailed");
    EXPECT_FALSE(result.diagnostics.front().message.empty());
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    std::ifstream input(sentinel);
    std::string contents;
    std::getline(input, contents);
    EXPECT_EQ(contents, "do not overwrite");
}

TEST(SagaHomeUiTest, AttachedEditorWindowNeverOwnsProductWindow)
{
    auto owner = std::make_unique<QMainWindow>();
    QPointer<QMainWindow> observed(owner.get());
    auto* stack = new QStackedWidget(owner.get());
    owner->setCentralWidget(stack);
    {
        SagaEditor::QtUIMainWindow editorWindow(*owner, *stack);
        editorWindow.DockPanel(new QWidget(), "saga.panel.viewport",
                               "World Viewport", SagaEditor::UIDockArea::Center);
        editorWindow.DockPanel(new QWidget(), "saga.panel.hierarchy",
                               "Hierarchy", SagaEditor::UIDockArea::Left);
        EXPECT_EQ(stack->count(), 1);
        EXPECT_NE(owner->findChild<QDockWidget*>("saga.panel.hierarchy"), nullptr);
    }
    EXPECT_FALSE(observed.isNull());
    EXPECT_EQ(stack->count(), 0);
    EXPECT_TRUE(owner->findChildren<QDockWidget*>().empty());
}

TEST(SagaHomeUiTest, AttachedWorkflowTimerDoesNotOutliveEditorAdapter)
{
    QMainWindow owner;
    auto* stack = new QStackedWidget(&owner);
    owner.setCentralWidget(stack);
    const auto timerCount = [&owner]
    {
        return owner.findChildren<QTimer*>(kWorkflowStateTimerObjectName).size();
    };
    const qsizetype baseline = timerCount();

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        int callbackCount = 0;
        {
            SagaEditor::QtUIMainWindow editorWindow(owner, *stack);
            editorWindow.SetOnWorkflowStatePoll(
                [&callbackCount] { ++callbackCount; });
            const auto timers =
                owner.findChildren<QTimer*>(kWorkflowStateTimerObjectName);
            ASSERT_EQ(timers.size(), baseline + 1);
            ASSERT_TRUE(QMetaObject::invokeMethod(
                timers.back(), "timeout", Qt::QueuedConnection));
        }

        QApplication::processEvents();
        EXPECT_EQ(callbackCount, 0);
        EXPECT_EQ(timerCount(), baseline);
    }
}

TEST(SagaHomeUiTest, AttachedEditorLeavesClosePolicyWithProductWindow)
{
    CloseOwningMainWindow owner;
    auto* stack = new QStackedWidget(&owner);
    owner.setCentralWidget(stack);
    int editorCloseCallbackCount = 0;
    SagaEditor::QtUIMainWindow editorWindow(owner, *stack);
    editorWindow.SetOnClose([&editorCloseCallbackCount]
    {
        ++editorCloseCallbackCount;
        return true;
    });

    owner.close();

    EXPECT_EQ(owner.closeEventCount, 1);
    EXPECT_EQ(editorCloseCallbackCount, 0);
}
} // namespace
